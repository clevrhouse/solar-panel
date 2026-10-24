"""Pure MQTT inventory and sensor planning; no HA dependencies or credentials."""
import json
import math
import re
import time

SERVICES = {'system', 'vebus', 'solarcharger', 'pvinverter', 'grid', 'battery'}
# `settings` carries only the two ESS limits the panel shows. It is never scanned
# (the settings tree is large); the runtime subscribes to those exact topics.
INGEST_SERVICES = SERVICES | {'settings'}
LIVE_MAX_AGE = 65   # seconds; same limit the panel used for Cerbo values
FORECAST_KEYS = ('today', 'remaining', 'this_hour', 'next_hour', 'tomorrow')
SETTINGS_PATHS = ('Settings/CGwacs/BatteryLife/MinimumSocLimit', 'Settings/CGwacs/BatteryLife/SocLimit')
ROLES = ('mppt', 'ac_pv', 'import', 'export', 'charge', 'discharge')


def number(value):
    return isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value)


def namespace(prefix, portal):
    if not re.fullmatch(r'[A-Za-z0-9_-]{1,80}', portal):
        raise ValueError('Invalid portal ID')
    if not re.fullmatch(r'[A-Za-z0-9_/-]{0,120}', prefix):
        raise ValueError('Invalid MQTT prefix')
    prefix = prefix.rstrip('/')+'/' if prefix else ''
    return prefix+'N/'+portal+'/'


def allowed(path):
    return path in SETTINGS_PATHS or path in {'ProductId','ProductName','CustomName','Position','State','Connected',
                    'Yield/System','Yield/User','Yield/Power','Ac/Power','FroniusDeviceType','Ac/NumberOfPhases',
                    'History/ChargedEnergy','History/DischargedEnergy'} or bool(re.fullmatch(
        r'(History/Daily/[0-7]/Yield|Ac/(Energy/(Forward|Reverse)|L[123]/(Power|Voltage|Energy/(Forward|Reverse)))|'
        r'Ac/(Grid|ConsumptionOnInput|ConsumptionOnOutput|PvOnGrid|PvOnOutput|PvOnGenset)/(NumberOfPhases|L[123]/Power)|'
        r'Ac/In/[01]/(DeviceInstance|Source)|Dc/Battery/(Power|Soc|Voltage|Current|TimeToGo)|Dc/0/(Voltage|Current))', path))


class Inventory:
    def __init__(self, prefix, portal):
        self.root = namespace(prefix, portal)
        self.values = {}
        self.devices = {}
        self.last_seen = None   # when the newest live (non-retained) GX message arrived

    def ingest(self, topic, payload, now=None, retained=False):
        if retained or not topic.startswith(self.root):
            return False
        parts = topic[len(self.root):].split('/', 2)
        if len(parts) != 3 or parts[0] not in INGEST_SERVICES or not re.fullmatch(r'[0-9]{1,6}',parts[1]):
            return False
        service, instance, path = parts
        if not allowed(path) or len(payload) > 8192:
            return False
        try:
            value = json.loads(payload).get('value') if payload else None
        except (ValueError, AttributeError, TypeError):
            return False
        key = service+'/'+instance
        d = self.devices.setdefault(key, {'service':service, 'instance':int(instance), 'paths':[]})
        if value is not None and path not in d['paths']:
            d['paths'].append(path)
        if path in ('ProductName','CustomName') and isinstance(value,str):
            # Store only presentation data, not arbitrary settings/serials.
            d[path] = ''.join(c for c in value[:80] if c.isprintable())
        if path == 'Position' and value in (0,1,2) and not isinstance(value,bool):
            d['position'] = value
        if path == 'Ac/NumberOfPhases' and type(value) is int and value in (1,2,3):
            d['phases'] = list(range(1,value+1))
        self.values[key+'/'+path] = (value, time.time() if now is None else now)
        self.last_seen = self.values[key+'/'+path][1]
        return True

    def age(self, now=None):
        """Seconds since the newest accepted GX message; None if none has arrived."""
        if self.last_seen is None:
            return None
        return max(0.0, (time.time() if now is None else now)-self.last_seen)

    def value(self, path, now=None, max_age=90):
        value, at = self.values.get(path,(None,0))
        now = time.time() if now is None else now
        return value if 0 <= now-at <= max_age else None

    def numeric(self, path, now=None, max_age=90):
        value = self.value(path,now,max_age)
        return float(value) if number(value) else None

    def phases(self, branch):
        base = 'system/0/Ac/'+branch
        n = self.value(base+'/NumberOfPhases')
        if type(n) is int and n in (1,2,3):
            return list(range(1,n+1))
        return [i for i in (1,2,3) if self.numeric(base+f'/L{i}/Power') is not None]

    def export(self):
        return {'devices':list(self.devices.values()), 'phases':{
            k:self.phases(k) for k in ('Grid','ConsumptionOnInput','ConsumptionOnOutput')}}


def source_id(path):
    return re.sub(r'[^a-z0-9_]', '_', path.lower())


def plan(inventory, selected):
    """Pick disjoint counters. Never add a total and its phase constituents."""
    devices = inventory['devices']
    phases = inventory['phases']
    specs = {}
    groups = {role:[] for role in ROLES}
    warnings = []

    def add(path, unit='kWh', counter=True):
        key=source_id(path)
        specs[key]={'key':key,'path':path,'unit':unit,'counter':counter or specs.get(key,{}).get('counter',False)}
        return key

    def counter(d, role, candidates):
        paths=d['paths']; base=d['service']+'/'+str(d['instance'])+'/'
        for candidate in candidates:
            if candidate in paths:
                groups[role].append(add(base+candidate)); return
        # Only use phase energy if the corresponding phase mask was confirmed.
        mask=phases.get('Grid',[]) if d['service']=='grid' else d.get('phases',[])
        direction='Reverse' if role=='export' else 'Forward'
        paths_needed=[f'Ac/L{i}/Energy/{direction}' for i in mask]
        if paths_needed and all(p in paths for p in paths_needed):
            groups[role].extend(add(base+p) for p in paths_needed); return
        groups[role].append(None)
        warnings.append(f"No supported {role} energy counter for {base[:-1]}; energy remains unavailable")

    for d in devices:
        service=d['service']; instance=d['instance']; base=service+'/'+str(instance)+'/'
        if service=='solarcharger':
            counter(d,'mppt',['Yield/System','Yield/User','History/Daily/0/Yield'])
            add(base+'Yield/Power','W',False)
            for day in range(8):
                path=f'History/Daily/{day}/Yield'
                if path in d['paths']: add(base+path,'kWh',False)
        elif service=='pvinverter':
            counter(d,'ac_pv',['Ac/Energy/Forward'])
            add(base+'Ac/Power','W',False)
        elif service=='grid' and instance==selected.get('grid'):
            counter(d,'import',['Ac/Energy/Forward'])
            counter(d,'export',['Ac/Energy/Reverse'])
        elif service=='battery' and instance==selected.get('battery'):
            counter(d,'charge',['History/ChargedEnergy'])
            counter(d,'discharge',['History/DischargedEnergy'])
    for role in ('import','export','charge','discharge'):
        if not groups[role]:
            groups[role]=[None]
            warnings.append('No selected native '+role+' energy source')
        if None in groups[role]:
            key = 'estimated_'+role+'_energy'
            specs[key] = {'key': key, 'path': '', 'unit': 'kWh', 'counter': True,
                          'estimated_role': role, 'phases': phases.get('Grid', [])}
            groups[role] = [key]
            warnings.append(role+': estimated from power while HA is running; no backfill across HA outages')
    return {'specs':list(specs.values()),'groups':groups,'warnings':warnings}


def runtime_paths(data):
    """Exact service/instance/path topics the entities read; nothing else."""
    paths = {spec['path'] for spec in data['plan']['specs'] if spec.get('path')}
    if any('estimated_role' in spec for spec in data['plan']['specs']):
        paths.add('system/0/Dc/Battery/Power')
        paths.update(f'system/0/Ac/Grid/L{i}/Power' for i in (1, 2, 3))
    for d in data['inventory']['devices']:
        base = d['service']+'/'+str(d['instance'])+'/'
        if d['service'] == 'solarcharger':
            paths.add(base+'Yield/Power')
            paths.update(base+f'History/Daily/{day}/Yield' for day in range(8))
        elif d['service'] == 'pvinverter':
            paths.add(base+'Ac/Power')
    paths.update(panel_paths(data))
    return sorted(paths)


def lowest(devices, service, prefer_position=None):
    """Lowest instance of a service ('prefer_position' wins when any device has it)."""
    found = [d for d in devices if d['service'] == service and type(d.get('instance')) is int]
    if prefer_position is not None and any(d.get('position') == prefer_position for d in found):
        found = [d for d in found if d.get('position') == prefer_position]
    return min((d['instance'] for d in found), default=None)


def panel_devices(data):
    """Instances the panel's direct (single-device) readings use."""
    devices = data['inventory']['devices']
    chosen = data['inventory'].get('selection', {}).get('vebus')
    buses = [d['instance'] for d in devices if d['service'] == 'vebus']
    return {'mppt': lowest(devices, 'solarcharger'),
            'vebus': chosen if chosen in buses else lowest(devices, 'vebus'),
            'pvinverter': lowest(devices, 'pvinverter', prefer_position=1)}


def panel_paths(data):
    """Exact topics behind the live snapshot, beyond the energy sensors' own."""
    picked = panel_devices(data)
    paths = {'system/0/Dc/Battery/'+k for k in ('Soc', 'Voltage', 'Power', 'Current', 'TimeToGo')}
    for branch in ('Grid', 'ConsumptionOnInput', 'ConsumptionOnOutput'):
        paths.update(f'system/0/Ac/{branch}/L{i}/Power' for i in (1, 2, 3))
    paths.update('settings/0/'+p for p in SETTINGS_PATHS)
    if picked['mppt'] is not None:
        base = f"solarcharger/{picked['mppt']}/"
        paths.update(base+p for p in ('Yield/Power', 'History/Daily/0/Yield', 'Dc/0/Voltage', 'Dc/0/Current', 'State'))
    if picked['vebus'] is not None:
        paths.add(f"vebus/{picked['vebus']}/State")
    if picked['pvinverter'] is not None:
        paths.update(f"pvinverter/{picked['pvinverter']}/"+p for p in ('Ac/Power', 'FroniusDeviceType'))
    return paths


def phase_mask(phases):
    return sum(1 << (i-1) for i in phases)


def panel_config(data, capacity):
    """Topology the firmware used to take from compile-time substitutions."""
    devices = data['inventory']['devices']
    phases = data['inventory']['phases']
    return {'gm': phase_mask(phases.get('Grid', [])),
            'am': phase_mask(phases.get('ConsumptionOnInput', [])),
            'cm': phase_mask(phases.get('ConsumptionOnOutput', [])),
            'pvn': sum(d['service'] == 'pvinverter' for d in devices),
            # A battery monitor is a `battery` service, but a MultiPlus/Quattro or other
            # VE.Bus system reports the battery only through the `system` service.
            'bat': any(d['service'] == 'battery' or (d['service'] == 'system' and 'Dc/Battery/Soc' in d.get('paths', []))
                       for d in devices),
            'cap': round(float(capacity), 2)}


def live_snapshot(inv, data, at, forecast, capacity, forecast_at=None, now=None, connected=True):
    """The one document the generic panel firmware reads (schema v1).

    Values older than LIVE_MAX_AGE become null, so the panel's own staleness
    handling needs no knowledge of MQTT. Keys are short on purpose: this is
    parsed on an ESP32.

    `mq` says whether HA's MQTT client is connected; while it is not, every GX
    value is null whatever was received earlier. `ga` is the age in seconds of
    the newest GX message (null: none since HA started), so the panel can tell
    "HA lost MQTT" from "the Cerbo stopped publishing" from "all fine".
    """
    picked = panel_devices(data)
    devices = data['inventory']['devices']

    def n(path):
        value = inv.numeric(path, now, LIVE_MAX_AGE)
        return None if value is None else round(value, 3)

    def code(path):
        value = inv.value(path, now, LIVE_MAX_AGE)
        return value if type(value) is int else None

    def phase_set(branch):
        return [n(f'system/0/Ac/{branch}/L{i}/Power') for i in (1, 2, 3)]

    m, v, p = picked['mppt'], picked['vebus'], picked['pvinverter']
    mppt = (lambda tail: n(f'solarcharger/{m}/{tail}')) if m is not None else (lambda tail: None)
    live = {
        'pv': mppt('Yield/Power'), 'yd': mppt('History/Daily/0/Yield'),
        'mv': mppt('Dc/0/Voltage'), 'mi': mppt('Dc/0/Current'),
        'ms': code(f'solarcharger/{m}/State') if m is not None else None,
        'soc': n('system/0/Dc/Battery/Soc'), 'bv': n('system/0/Dc/Battery/Voltage'),
        'bp': n('system/0/Dc/Battery/Power'), 'bi': n('system/0/Dc/Battery/Current'),
        'ttg': n('system/0/Dc/Battery/TimeToGo'),
        'g': phase_set('Grid'), 'a': phase_set('ConsumptionOnInput'), 'c': phase_set('ConsumptionOnOutput'),
        'vs': code(f'vebus/{v}/State') if v is not None else None,
        'pw': n(f'pvinverter/{p}/Ac/Power') if p is not None else None,
        'fr': p is not None and inv.value(f'pvinverter/{p}/FroniusDeviceType', now, LIVE_MAX_AGE) is not None,
        'mn': n('settings/0/Settings/CGwacs/BatteryLife/MinimumSocLimit'),
        'ma': n('settings/0/Settings/CGwacs/BatteryLife/SocLimit'),
    }
    if not connected:
        live = {k: ([None]*3 if isinstance(x, list) else False if k == 'fr' else None) for k, x in live.items()}
    aggregate = {
        'mp': aggregate_power(inv, devices, 'solarcharger', None, LIVE_MAX_AGE, now),
        'ap': aggregate_power(inv, devices, 'pvinverter', None, LIVE_MAX_AGE, now),
        'ao': aggregate_power(inv, devices, 'pvinverter', 1, LIVE_MAX_AGE, now)}
    if not connected:
        aggregate = dict.fromkeys(aggregate)
    cfg = panel_config(data, capacity)
    cfg['bat'] = cfg['bat'] or live['soc'] is not None
    age = inv.age(now)
    return {'v': 1, 'at': int(at), 'mq': 1 if connected else 0, 'ga': None if age is None else int(age),
            'cfg': cfg, 'L': live,
            'H': {k: None if x is None else round(x, 3) for k, x in aggregate.items()},
            'F': [None if x is None else round(x, 3) for x in forecast],
            'fu': int(forecast_at) if forecast_at else None}


def strict_sum(values):
    values=list(values)
    return sum(values) if all(number(v) for v in values) else None


def aggregate_power(inv, devices, service, position=None, max_age=90, now=None):
    selected=[d for d in devices if d['service']==service and (position is None or d.get('position')==position)]
    if position is not None and any(d['service']==service and 'position' not in d for d in devices):
        return None
    path='Yield/Power' if service=='solarcharger' else 'Ac/Power'
    return strict_sum(inv.numeric(f"{service}/{d['instance']}/{path}", now, max_age) for d in selected)


class EnergyIntegral:
    """Left-hand power integration, with no integration across missing samples."""
    def __init__(self, value=0):
        self.value, self.at, self.power = value, None, None

    def update(self, power, at):
        if self.at is not None and number(power) and number(self.power) and 0 <= at-self.at <= 90:
            self.value += self.power*(at-self.at)/3600000
        self.at, self.power = at, power
        return self.value
