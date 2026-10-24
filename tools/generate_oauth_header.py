#!/usr/bin/env python3
"""Embed maintainer-provided OAuth client data without logging it."""
import json
import os
from pathlib import Path

target = Path("include/oauth_client.generated.hpp")
client_id = os.environ.get("SAVENX_GOOGLE_CLIENT_ID", "").strip()
client_secret = os.environ.get("SAVENX_GOOGLE_CLIENT_SECRET", "").strip()
if client_id:
    if not client_id.endswith(".apps.googleusercontent.com"):
        raise SystemExit("SAVENX_GOOGLE_CLIENT_ID has an invalid format")
    target.write_text("#pragma once\nnamespace savenx::oauth {\n"
                      f"inline constexpr const char *CLIENT_ID = {json.dumps(client_id)};\n"
                      f"inline constexpr const char *CLIENT_SECRET = {json.dumps(client_secret)};\n" + "}\n")
    print("OAuth client included in this build.")
else:
    target.unlink(missing_ok=True)
    print("No build OAuth client. Configure the client in the app or on the SD.")
