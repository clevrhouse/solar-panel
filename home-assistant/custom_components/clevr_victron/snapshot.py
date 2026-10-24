"""Recorder-backed display feeds, independent of panel uptime."""
import json
from datetime import timedelta
from homeassistant.components.recorder import get_instance
from homeassistant.components.recorder.statistics import statistics_during_period
from homeassistant.components.sensor import SensorEntity
from homeassistant.helpers import entity_registry as er
from homeassistant.util import dt as dt_util
from .const import DOMAIN
from .history import combine
from .model import ROLES, strict_sum

SCAN_INTERVAL = timedelta(seconds=60)




class Snapshot(SensorEntity):
    _attr_icon = 'mdi:chart-bar'
    _unrecorded_attributes = frozenset({'snapshot'})

    def __init__(self, entry, inv, energy):
        self.entry, self.inv, self.energy = entry, inv, energy
        key = 'energy_snapshot' if energy else 'history_snapshot'
        self._attr_unique_id = entry.data['portal']+'_'+key
        self._attr_name = 'Clevr Victron '+key.replace('_', ' ')
        self.entity_id = 'sensor.clevr_victron_'+key

    async def async_update(self):
        now = dt_util.now()
        registry = er.async_get(self.hass)
        groups = {role: [registry.async_get_entity_id('sensor', DOMAIN, self.entry.data['portal']+'_'+key)
                         if key else None for key in keys]
                  for role, keys in self.entry.data['plan']['groups'].items()}
        ids = {entity for items in groups.values() for entity in items if entity}
        midnight = now.replace(hour=0, minute=0, second=0, microsecond=0)

        def read():
            hourly = statistics_during_period(self.hass, dt_util.as_utc(midnight-timedelta(days=7, hours=1)),
                dt_util.as_utc(now), ids, 'hour', {'energy': 'kWh'}, {'sum'}) if ids else {}
            recent = statistics_during_period(self.hass, dt_util.as_utc(now)-timedelta(hours=3),
                dt_util.as_utc(now), ids, '5minute', {'energy': 'kWh'}, {'sum'}) if ids else {}
            return combine(now, groups, hourly, recent)

        data = await get_instance(self.hass).async_add_executor_job(read)
        chargers = [d for d in self.entry.data['inventory']['devices'] if d['service'] == 'solarcharger']
        # GX daily totals can backfill daily MPPT bars, never fabricated hourly bars.
        if chargers:
            for index in range(8):
                values = []
                for charger in chargers:
                    path = f"solarcharger/{charger['instance']}/History/Daily/{7-index}/Yield"
                    value = self.inv.numeric(path)
                    received = self.inv.values.get(path, (None, 0))[1]
                    if dt_util.as_local(dt_util.utc_from_timestamp(received)).date() != now.date():
                        value = None
                    values.append(value if value is not None and value >= 0 else None)
                daily = strict_sum(values)
                if daily is not None:
                    data['days'][index][0] = daily
        if self.energy:
            data = {'v': 1, 'at': int(now.timestamp()), 'meters': [
                {'today': data['days'][7][i], 'last': data['days'][6][i], 'reset': int(midnight.timestamp())}
                for i in range(6)]}
        self._attr_native_value = now.isoformat()
        self._attr_extra_state_attributes = {'snapshot': json.dumps(data, separators=(',', ':'), allow_nan=False)}
