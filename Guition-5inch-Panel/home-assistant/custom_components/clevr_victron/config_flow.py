"""Discover locally, then require review before creating entities."""
import logging
import voluptuous as vol
from homeassistant import config_entries
from homeassistant.core import callback
from homeassistant.components import mqtt
from homeassistant.helpers import selector
from homeassistant.exceptions import HomeAssistantError
from .const import DOMAIN, DEFAULT_CAPACITY
from .model import namespace, plan, FORECAST_KEYS
from .scan import scan

_LOGGER = logging.getLogger(__name__)


class Flow(config_entries.ConfigFlow, domain=DOMAIN):
    VERSION = 1

    async def async_step_user(self, user_input=None):
        errors = {}
        # The panel firmware reads fixed entity IDs (sensor.clevr_victron_*), so one GX per HA.
        if self._async_current_entries():
            return self.async_abort(reason='single_gx')
        if user_input is not None:
            # Outside the try: AbortFlow is a HomeAssistantError and must reach
            # the UI as "already configured", not as a discovery failure.
            await self.async_set_unique_id(user_input['portal'])
            self._abort_if_unique_id_configured()
            try:
                namespace(user_input['prefix'], user_input['portal'])
                if not mqtt.is_connected(self.hass):
                    raise ValueError('MQTT disconnected')
                self.settings = user_input
                self.inventory = await scan(self.hass, user_input['prefix'],
                                            user_input['portal'])
                return await self.async_step_review()
            except (ValueError, HomeAssistantError) as err:
                _LOGGER.warning('GX discovery failed: %s', err)
                errors['base'] = 'cannot_discover'
        return self.async_show_form(step_id='user', errors=errors, data_schema=vol.Schema({
            vol.Required('prefix', default='victron/'): str,
            vol.Required('portal'): str,
        }))

    async def async_step_review(self, user_input=None):
        if user_input is not None:
            self.inventory['devices'] = [d for d in self.inventory['devices']
                if d['service'] not in ('solarcharger', 'pvinverter') or
                f"{d['service']}/{d['instance']}" in user_input['solar_devices']]
            for branch in ('Grid', 'ConsumptionOnInput', 'ConsumptionOnOutput'):
                self.inventory['phases'][branch] = [int(i) for i in user_input[branch]]
            selected = {k: int(user_input[k]) if user_input[k] != 'none' else None
                        for k in ('grid', 'battery')}
            if 'vebus' in user_input:
                selected['vebus'] = int(user_input['vebus'])
            self.settings = {**self.settings, 'battery_capacity_kwh': user_input['battery_capacity_kwh']}
            self.inventory['selection'] = selected
            self.result = {
                **self.settings, 'inventory': self.inventory,
                'plan': plan(self.inventory, selected)}
            return await self.async_step_confirm()
        fields = {}
        for service in ('grid', 'battery'):
            options = {'none': 'No native counter (energy unavailable)'}
            options.update({str(d['instance']): d.get('ProductName', service)+' '+str(d['instance'])
                            for d in self.inventory['devices'] if d['service'] == service})
            default = next(iter(options.keys()-{'none'})) if len(options) == 2 else 'none'
            fields[vol.Required(service, default=default)] = vol.In(options)
        buses = sorted(d['instance'] for d in self.inventory['devices'] if d['service'] == 'vebus')
        if len(buses) > 1:
            # The panel shows one inverter/charger state; the lowest instance is the default.
            fields[vol.Required('vebus', default=str(buses[0]))] = vol.In({str(b): 'VE.Bus '+str(b) for b in buses})
        fields[vol.Required('battery_capacity_kwh', default=DEFAULT_CAPACITY)] = vol.All(
            vol.Coerce(float), vol.Range(min=0.1, max=10000))
        solar = [f"{d['service']}/{d['instance']}" for d in self.inventory['devices']
                 if d['service'] in ('solarcharger', 'pvinverter')]
        fields[vol.Required('solar_devices', default=solar)] = selector.SelectSelector(
            selector.SelectSelectorConfig(options=solar, multiple=True))
        for branch, mask in self.inventory['phases'].items():
            fields[vol.Required(branch, default=[str(i) for i in mask])] = selector.SelectSelector(
                selector.SelectSelectorConfig(options=['1', '2', '3'], multiple=True))
        return self.async_show_form(step_id='review', data_schema=vol.Schema(fields),
            description_placeholders={'inventory': ', '.join(solar) or 'No solar devices detected'})

    @staticmethod
    @callback
    def async_get_options_flow(config_entry):
        return OptionsFlow(config_entry)

    async def async_step_confirm(self, user_input=None):
        if user_input is not None:
            return self.async_create_entry(title='Clevr GX '+self.settings['portal'], data=self.result)
        warnings = self.result['plan']['warnings']
        return self.async_show_form(step_id='confirm', data_schema=vol.Schema({}),
            description_placeholders={'warnings': '\n'.join(warnings) or 'All selected energy sources have native counters.'})


class OptionsFlow(config_entries.OptionsFlow):
    """Battery capacity and optional forecast overrides; no rescan needed."""

    def __init__(self, entry):
        self._entry = entry

    async def async_step_init(self, user_input=None):
        current = {**self._entry.data, **self._entry.options}
        if user_input is not None:
            # Cleared pickers are simply absent: they fall back to auto-detection.
            return self.async_create_entry(title='', data={k: v for k, v in user_input.items() if v not in (None, '')})
        fields = {vol.Required('battery_capacity_kwh', default=current.get('battery_capacity_kwh', DEFAULT_CAPACITY)):
                  vol.All(vol.Coerce(float), vol.Range(min=0.1, max=10000))}
        for key in FORECAST_KEYS:
            chosen = current.get('forecast_'+key)
            marker = vol.Optional('forecast_'+key, description={'suggested_value': chosen} if chosen else None)
            fields[marker] = selector.EntitySelector(selector.EntitySelectorConfig(domain='sensor'))
        return self.async_show_form(step_id='init', data_schema=vol.Schema(fields))
