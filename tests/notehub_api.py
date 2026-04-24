"""
Shared Notehub REST API helpers for integration tests.

Provides helpers for querying the Notehub API and verifying events.
Stdlib-only — no pip dependencies.
"""

import json
import sys
import time
import urllib.error
import urllib.request

NOTEHUB_API = "https://api.notefile.net"


def notehub_get(path, token):
    """GET a Notehub API endpoint. Returns parsed JSON or None."""
    url = f"{NOTEHUB_API}{path}"
    req = urllib.request.Request(url, headers={
        "Authorization": f"Bearer {token}",
    })
    try:
        with urllib.request.urlopen(req, timeout=30) as resp:
            return json.loads(resp.read())
    except urllib.error.HTTPError as e:
        if e.code == 429:
            # Rate limited — back off silently
            time.sleep(5)
            return None
        print(f"  Notehub API error: HTTP {e.code} {e.reason}", file=sys.stderr)
        return None
    except urllib.error.URLError as e:
        print(f"  Notehub API error: {e.reason}", file=sys.stderr)
        return None


def verify_event(token, product_uid, device_uid, test_id, timeout_s=120, poll_s=5):
    """Poll Notehub events until we find one matching test_id."""
    print(f"Verifying event in Notehub (timeout {timeout_s}s)...")
    print(f"  product:  {product_uid}")
    print(f"  device:   {device_uid}")
    print(f"  test_id:  {test_id}")

    path = (
        f"/v1/projects/{product_uid}/events"
        f"?files=test.qo&deviceUID={device_uid}"
        f"&sortOrder=desc&pageSize=10"
    )

    deadline = time.time() + timeout_s
    attempts = 0
    while time.time() < deadline:
        attempts += 1
        data = notehub_get(path, token)
        if data and "events" in data:
            for event in data["events"]:
                body = event.get("body", {})
                if body.get("test_id") == test_id:
                    print(f"\n  FOUND event after {attempts} poll(s):")
                    print(f"    event UID: {event.get('uid', 'n/a')}")
                    print(f"    received:  {event.get('received', 'n/a')}")
                    print(f"    body:      {json.dumps(body)}")
                    return True
        print(f"  poll {attempts}: not yet...", end="\r")
        # Back off more on rate limiting (429)
        time.sleep(poll_s)

    print(f"\n  Event not found after {attempts} polls ({timeout_s}s)")
    return False
