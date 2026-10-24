"""Native HA entities from reviewed GX inventory; HA owns all persistence."""
import json
import logging
import re
import time
from datetime import timedelta
from homeassistant.components import mqtt
from homeassistant.components.sensor import RestoreSensor, SensorDeviceClass, SensorEntity, SensorStateClass, SensorExtraStoredData
from homeassistant.core import callback
from homeassistant.exceptions import ConfigEntryError
from homeassistant.helpers import entity_registry as er
from homeassistant.helpers import issue_registry as ir
from homeassistant.helpers.event import async_track_time_interval
from .const import DOMAIN, DEFAULT_CAPACITY, SNAPSHOT_MIN_INTERVAL, SNAPSHOT_HEARTBEAT
from .model import (Inventory, aggregate_power, EnergyIntegral, strict_sum, number, runtime_paths,
                    live_snapshot, FORECAST_KEYS)

_LOGGER = logging.getLogger(__name__)
SCAN_INTERVAL = timedelta(seconds=60)


async def async_setup_entry(hass, entry, async_add_entities):
    inv = Inventory(entry.data['prefix'], entry.data['portal'])
    entities = [Source(entry, inv, spec) for spec in entry.data['plan']['specs']]
    entities += [Aggregate(entry, inv, key, service, position) for key, service, position in (
        ('mppt_power', 'solarcharger', None), ('ac_pv_power', 'pvinverter', None),
        ('ac_pv_output_power', 'pvinverter', 1))]

    @callback
    def receive(msg):
        inv.ingest(msg.topic, msg.payload, retained=msg.retain)

    paths = runtime_paths(entry.data)
    live = LiveSnapshot(hass, entry, inv)
    async def refresh(now):
        for entity in entities:
            if entity.hass is not None:
                entity.advance()
                entity.async_write_ha_state()

    # Normal operation subscribes only to the reviewed sensors, not the tree.
    for path in paths:
        entry.async_on_unload(await mqtt.async_subscribe(hass, inv.root+path, receive, qos=0))
    entry.async_on_unload(async_track_time_interval(hass, refresh, timedelta(seconds=5)))
    entry.async_on_unload(async_track_time_interval(hass, live.tick, timedelta(seconds=SNAPSHOT_MIN_INTERVAL)))
    entry.async_on_unload(entry.add_update_listener(_reload))
    from .snapshot import Snapshot
    async_add_entities(entities + [Snapshot(entry, inv, False), Snapshot(entry, inv, True), live])
    await refresh(None)


async def _reload(hass, entry):
    await hass.config_entries.async_reload(entry.entry_id)


FIXED_IDS = ('energy_snapshot', 'history_snapshot', 'live_snapshot')


def migrate_entity_ids(hass, entry):
    """Give the panel feeds fixed entity IDs (sensor.clevr_victron_*).

    The firmware no longer carries a per-site portal ID, so it reads these names.
    Entries created before this release registered portal-specific IDs; rename
    them once, unless the target ID is already taken.
    """
    registry = er.async_get(hass)
    for key in FIXED_IDS:
        current = registry.async_get_entity_id('sensor', DOMAIN, entry.data['portal']+'_'+key)
        target = 'sensor.clevr_victron_'+key
        if current and current != target and registry.async_get(target) is None and hass.states.get(target) is None:
            registry.async_update_entity(current, new_entity_id=target)


def feed_conflicts(hass, entry):
    """Fixed feed IDs this entry does not own but something else does (or HA would suffix)."""
    registry = er.async_get(hass)
    taken = []
    for key in FIXED_IDS:
        target = 'sensor.clevr_victron_'+key
        mine = registry.async_get_entity_id('sensor', DOMAIN, entry.data['portal']+'_'+key)
        if mine == target:
            continue
        if registry.async_get(target) is not None or hass.states.get(target) is not None:
            taken.append(target)
    return taken


def check_feed_ownership(hass, entry):
    """Raise ConfigEntryError unless this entry may own the panel feed entity IDs.

    The firmware reads fixed IDs (sensor.clevr_victron_*), so exactly one GX entry
    may own them and nothing else may sit on those names. Runs before any platform
    is set up, so a refusal leaves the entry in a visible failed state with a repair.
    """
    first = hass.config_entries.async_entries(DOMAIN)[0]
    if first.entry_id != entry.entry_id:
        ir.async_create_issue(hass, DOMAIN, 'second_gx_'+entry.entry_id, is_fixable=False,
                              severity=ir.IssueSeverity.ERROR, translation_key='second_gx',
                              translation_placeholders={'portal': entry.data['portal']})
        raise ConfigEntryError('The Clevr panels read one GX; "%s" is not the first entry. Remove this entry.' % entry.data['portal'])
    ir.async_delete_issue(hass, DOMAIN, 'second_gx_'+entry.entry_id)
    migrate_entity_ids(hass, entry)
    taken = feed_conflicts(hass, entry)
    if taken:
        ir.async_create_issue(hass, DOMAIN, 'feed_name_conflict', is_fixable=False,
                              severity=ir.IssueSeverity.ERROR, translation_key='feed_name_conflict',
                              translation_placeholders={'ids': ', '.join(taken)})
        raise ConfigEntryError('Entity IDs already in use by something else: '+', '.join(taken))
    ir.async_delete_issue(hass, DOMAIN, 'feed_name_conflict')


FORECAST_PATTERNS = {
    'today': r'estimated_energy_production_today',
    'remaining': r'estimated_energy_production_today_remaining',
    'this_hour': r'estimated_energy_production_current_hour',
    'next_hour': r'estimated_energy_production_next_hour',
    'tomorrow': r'estimated_energy_production_tomorrow'}


def forecast_entities(hass, options):
    """Entity per forecast slot: the user's choice, else the VRM integration's sensor."""
    found = {}
    registry = er.async_get(hass)
    for key in FORECAST_KEYS:
        chosen = options.get('forecast_'+key)
        if chosen:
            found[key] = chosen
            continue
        pattern = re.compile(r'sensor\.[a-z0-9_]*'+FORECAST_PATTERNS[key]+r'(_\d+)?')
        known = {e.entity_id for e in registry.entities.values() if e.domain == 'sensor' and not e.disabled}
        known.update(hass.states.async_entity_ids('sensor'))
        matches = sorted(e for e in known if pattern.fullmatch(e))
        if matches:
            found[key] = matches[0]
    return found


def forecast_kwh(hass, entity_id):
    state = hass.states.get(entity_id) if entity_id else None
    if state is None or state.state in ('unknown', 'unavailable', ''):
        return None
    try:
        value = float(state.state)
    except ValueError:
        return None
    unit = state.attributes.get('unit_of_measurement')
    if unit == 'Wh':
        value /= 1000
    elif unit not in (None, 'kWh'):
        return None
    return value if number(value) and value >= 0 else None


class LiveSnapshot(SensorEntity):
    """Everything the panel shows live, in one attribute (replaces the panel's MQTT).

    Published when the content changes (at most every SNAPSHOT_MIN_INTERVAL) and
    at least every SNAPSHOT_HEARTBEAT so the panel can tell a quiet system from a
    dead feed. The attribute is excluded from the Recorder; consider excluding
    the entity too.
    """
    _attr_should_poll = False
    _attr_has_entity_name = False
    _attr_icon = 'mdi:transmission-tower'
    _unrecorded_attributes = frozenset({'snapshot'})

    def __init__(self, hass, entry, inv):
        self.hass_ref, self.entry, self.inv = hass, entry, inv
        self._attr_unique_id = entry.data['portal']+'_live_snapshot'
        self._attr_name = 'Clevr Victron live snapshot'
        self.entity_id = 'sensor.clevr_victron_live_snapshot'
        self._attr_device_info = {'identifiers': {(DOMAIN, entry.data['portal'])},
                                  'name': 'Clevr GX '+entry.data['portal'], 'manufacturer': 'Victron Energy'}
        self._last_body, self._last_sent = None, 0.0
        self._attr_native_value = None
        self._attr_extra_state_attributes = {}

    def build(self, at=None, connected=None):
        options = {**self.entry.data, **self.entry.options}
        ids = forecast_entities(self.hass_ref, options)
        forecast = [forecast_kwh(self.hass_ref, ids.get(k)) for k in FORECAST_KEYS]
        today = self.hass_ref.states.get(ids.get('today', ''))
        forecast_at = int(today.last_updated.timestamp()) if today is not None and forecast[0] is not None else None
        capacity = options.get('battery_capacity_kwh', DEFAULT_CAPACITY)
        if connected is None:
            connected = mqtt.is_connected(self.hass_ref)
        return live_snapshot(self.inv, self.entry.data, time.time() if at is None else at, forecast, capacity,
                             forecast_at, connected=connected)

    @callback
    def tick(self, _now=None, monotonic=None):
        if self.hass is None:
            return
        # Keep publishing while HA's MQTT is down: the snapshot then says so (mq=0,
        # no values), which is how the panel tells "feed down" from "HA down".
        snapshot = self.build()
        # `at` and `ga` change every tick; they must not defeat change detection.
        body = json.dumps({k: v for k, v in snapshot.items() if k not in ('at', 'ga')}, separators=(',', ':'), allow_nan=False)
        now = time.monotonic() if monotonic is None else monotonic
        if body == self._last_body and now-self._last_sent < SNAPSHOT_HEARTBEAT:
            return
        self._last_body, self._last_sent = body, now
        self._attr_native_value = 'ready'
        self._attr_extra_state_attributes = {'snapshot': json.dumps(snapshot, separators=(',', ':'), allow_nan=False)}
        self.async_write_ha_state()


class Source(RestoreSensor):
    _attr_should_poll = False
    _attr_has_entity_name = True

    def __init__(self, entry, inv, spec):
        self.inv, self.spec = inv, spec
        self.integral = EnergyIntegral() if 'estimated_role' in spec else None
        self._attr_unique_id = f"{entry.data['portal']}_{spec['key']}"
        self._attr_name = spec['key'].replace('_', ' ')
        self._attr_native_unit_of_measurement = spec['unit']
        self._attr_device_class = SensorDeviceClass.POWER if spec['unit'] == 'W' else SensorDeviceClass.ENERGY
        self._attr_state_class = (SensorStateClass.TOTAL_INCREASING if spec['counter']
                                 else SensorStateClass.MEASUREMENT if spec['unit'] == 'W' else None)
        self._attr_device_info = {'identifiers': {(DOMAIN, entry.data['portal'])},
                                 'name': 'Clevr GX '+entry.data['portal'], 'manufacturer': 'Victron Energy'}

    async def async_added_to_hass(self):
        await super().async_added_to_hass()
        if self.integral is not None:
            restored = await self.async_get_last_sensor_data()
            if restored and number(restored.native_value) and restored.native_value >= 0:
                self.integral.value = restored.native_value

    @property
    def extra_restore_state_data(self):
        # Preserve the accumulated counter even if the power source is offline
        # when HA shuts down; availability must not erase its stored energy.
        if self.integral is not None:
            return SensorExtraStoredData(self.integral.value, 'kWh')
        return super().extra_restore_state_data

    def power(self):
        role = self.spec['estimated_role']
        if role in ('charge', 'discharge'):
            value = self.inv.numeric('system/0/Dc/Battery/Power')
        else:
            mask = self.spec['phases']
            value = strict_sum(self.inv.numeric(f'system/0/Ac/Grid/L{i}/Power') for i in mask) if mask else None
        return max(0, value * (-1 if role in ('export', 'discharge') else 1)) if value is not None else None

    def advance(self):
        if self.integral is not None:
            self.integral.update(self.power() if mqtt.is_connected(self.hass) else None, time.monotonic())

    @property
    def native_value(self):
        if self.integral is not None:
            return self.integral.value if self.power() is not None else None
        value = self.inv.numeric(self.spec['path'])
        return value if value is not None and (self.spec['unit'] != 'kWh' or value >= 0) else None

    @property
    def available(self):
        return mqtt.is_connected(self.hass) and self.native_value is not None


class Aggregate(Source):
    def __init__(self, entry, inv, key, service, position):
        super().__init__(entry, inv, {'key': key, 'path': '', 'unit': 'W', 'counter': False})
        self.devices = entry.data['inventory']['devices']
        self.service, self.position = service, position

    @property
    def native_value(self):
        return aggregate_power(self.inv, self.devices, self.service, self.position)
