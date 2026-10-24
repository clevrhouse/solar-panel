"""Local GX discovery and Recorder sources. Never writes GX settings."""
from .const import DOMAIN, PLATFORMS


async def async_setup_entry(hass, entry):
    from .sensor import check_feed_ownership   # imports HA's MQTT component, so not at module load
    check_feed_ownership(hass, entry)
    await hass.config_entries.async_forward_entry_setups(entry, PLATFORMS)
    return True


async def async_unload_entry(hass, entry):
    return await hass.config_entries.async_unload_platforms(entry, PLATFORMS)
