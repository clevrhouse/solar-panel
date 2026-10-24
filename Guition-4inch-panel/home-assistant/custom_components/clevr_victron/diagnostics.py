"""Allowlisted discovery export for the website; no credentials or raw MQTT."""
from homeassistant.helpers import entity_registry as er
from .const import DOMAIN


async def async_get_config_entry_diagnostics(hass, entry):
    registry = er.async_get(hass)
    return {'schema': 1, 'release': '2.2.0-dev2', 'portal': entry.data['portal'],
            'prefix': entry.data['prefix'],
            'inventory': entry.data['inventory'],
            'plan': entry.data['plan'], 'entities': {
                item.unique_id.removeprefix(entry.data['portal']+'_'): item.entity_id
                for item in er.async_entries_for_config_entry(registry, entry.entry_id)
                if item.platform == DOMAIN}}
