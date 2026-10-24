"""Convert Recorder sums (kWh) to a disposable, bounded display snapshot.

Require adjacent statistics: an increase across a recording gap cannot be
attributed to one hour. Missing periods remain null, including daily totals
that would otherwise silently undercount. No database writes or local files.
"""
from datetime import datetime, timedelta, timezone
import math


def deltas(rows, step):
    points = {int(r["start"]): r.get("sum") for r in rows}
    result = {}
    for start, value in points.items():
        previous = points.get(start - step)
        if (isinstance(value, (int, float)) and isinstance(previous, (int, float))
                and math.isfinite(value) and math.isfinite(previous)):
            change = value - previous
            if change >= -1e-6:
                result[start] = max(0.0, change)
    return result


def complete_sum(values):
    values = list(values)
    return round(sum(values), 5) if values and all(v is not None for v in values) else None


def build_snapshot(now, entities, hourly, recent):
    """Eight local dates, 24 wall-clock hours today, six energy series.

    On autumn DST change the two occurrences of an hour are added together.
    The absent spring hour stays null. Use UTC when stepping physical hours.
    The open hour contains only complete five-minute statistics (0 in its first minutes, see below).
    """
    midnight = now.replace(hour=0, minute=0, second=0, microsecond=0)
    dates = [midnight - timedelta(days=7-i) for i in range(8)]
    hours = [[None] * 6 for _ in range(24)]
    days = [[None] * 6 for _ in range(8)]
    current_hour = int(now.timestamp()) // 3600 * 3600
    for col, entity in enumerate(entities):
        long = deltas(hourly.get(entity, []), 3600)
        short = deltas(recent.get(entity, []), 300)
        # The latest recorded five-minute interval, not the wall clock, is the
        # end of the partial current hour. Never invent zeros before it arrives.
        latest = max((s for s in short if current_hour <= s < now.timestamp()), default=None)
        # Recorder compiles an interval ~5 minutes after it starts, so for the first minutes of
        # every hour no five-minute row of the open hour exists yet. When the last interval of the
        # previous hour is there (recording is healthy and contiguous), the open hour has simply
        # produced nothing recorded so far: 0, not unknown. Otherwise (a gap) it stays null.
        just_opened = (latest is None and now.timestamp() - current_hour < 600
                       and short.get(current_hour - 300) is not None)
        for day_index, date in enumerate(dates):
            begin = int(date.timestamp())
            end = int((date + timedelta(days=1)).timestamp())
            values = []
            wall_hours = {}
            for start in range(begin, min(end, current_hour + 3600), 3600):
                if start < current_hour:
                    value = long.get(start)
                    if value is None:
                        value = complete_sum(short.get(s) for s in range(start, start+3600, 300))
                else:
                    if latest is not None:
                        value = complete_sum(short.get(s) for s in range(start, latest+300, 300))
                    else:
                        value = 0.0 if just_opened else None
                values.append(value)
                hour = datetime.fromtimestamp(start, timezone.utc).astimezone(now.tzinfo).hour
                wall_hours.setdefault(hour, []).append(value)
            days[day_index][col] = complete_sum(values)
            if day_index == 7:
                for hour, values in wall_hours.items():
                    hours[hour][col] = complete_sum(values)
    return {"v": 1, "at": int(now.timestamp()), "dates": [int(d.timestamp()) for d in dates],
            "hours": hours, "days": days}


def merge_mppt(snapshot, now, groups, hourly, recent, daily):
    """Per-charger statistics for hours; Cerbo totals for days.

    groups contains the eight entity IDs (Daily/0..7) for each charger.
    daily contains already validated, fresh HA states, never panel samples.
    No partial sum when one selected charger is missing.
    """
    per_charger = [build_snapshot(now, [group[0]]*6, hourly, recent) for group in groups]
    for hour in range(24):
        snapshot['hours'][hour][0] = complete_sum(s['hours'][hour][0] for s in per_charger)
    for index in range(8):
        offset = 7-index
        snapshot['days'][index][0] = complete_sum(daily.get(group[offset]) for group in groups)
    return snapshot


from .model import ROLES, strict_sum

def combine(now, groups, hourly, recent):
    """Aggregate per-device deltas; [] means reviewed absence, [None] missing."""
    result = build_snapshot(now, [], {}, {})
    for column, role in enumerate(ROLES):
        feeds = [build_snapshot(now, [entity], hourly, recent) if entity else None
                 for entity in groups[role]]
        for section in ('hours', 'days'):
            for index, row in enumerate(result[section]):
                row[column] = strict_sum(f[section][index][0] if f else None for f in feeds)
    return result

