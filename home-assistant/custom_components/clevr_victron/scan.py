"""Bounded discovery from the MQTT feed already configured in HA."""
import asyncio
import logging
from homeassistant.components import mqtt
from homeassistant.core import callback
from .model import Inventory, SERVICES

_LOGGER = logging.getLogger(__name__)

# Two complete publications can be nearly two keepalive intervals apart when
# subscriptions start just after a publish. Permit two 60 s intervals + margin.
SCAN_TIMEOUT = 130


async def scan(hass, prefix, portal):
    inv = Inventory(prefix, portal)
    completed = asyncio.Event()
    seen = 0

    @callback
    def receive(msg):
        nonlocal seen
        if msg.topic == inv.root+'full_publish_completed':
            if not msg.retain:
                # The first marker may end a dump we joined halfway; the second
                # ends one observed from its start.
                seen += 1
                if seen >= 2:
                    completed.set()
        else:
            inv.ingest(msg.topic, msg.payload, retained=msg.retain)

    topics = [inv.root+service+'/#' for service in sorted(SERVICES)] + [inv.root+'full_publish_completed']
    unsubscribes = []
    try:
        # No SUBACK wait: HA sends no new SUBSCRIBE (and no ack) for a topic it
        # already holds. The timeout below bounds the scan either way.
        for topic in topics:
            unsubscribes.append(await mqtt.async_subscribe(hass, topic, receive, qos=0))
        try:
            await asyncio.wait_for(completed.wait(), SCAN_TIMEOUT)
        except TimeoutError:
            pass
        _LOGGER.info('GX scan of %s: %d devices, %d completion markers',
                     inv.root, len(inv.devices), seen)
        result = inv.export()
        result['scan_complete'] = completed.is_set()
        if not result['devices'] or not completed.is_set():
            raise ValueError('No complete GX inventory; verify the existing HA MQTT feed, portal and prefix')
        return result
    finally:
        for unsubscribe in unsubscribes:
            unsubscribe()
