DOMAIN = 'clevr_victron'
PLATFORMS = ['sensor']
DEFAULT_CAPACITY = 10.0
SNAPSHOT_MIN_INTERVAL = 5     # seconds between content-change publishes
SNAPSHOT_HEARTBEAT = 20       # seconds; republish unchanged so the panel sees a live feed
