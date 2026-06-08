#!/usr/bin/env bash
set -euo pipefail

HOST="${1:-${ALPACA_HOST:-esp32alptherm1.local}}"
DEVICE="${ALPACA_DEVICE:-camera}"
DEVICE_NUMBER="${ALPACA_DEVICE_NUMBER:-0}"
CLIENT_ID="${ALPACA_CLIENT_ID:-4242}"
DURATION="${ALPACA_EXPOSURE_DURATION:-0.5}"
LIGHT="${ALPACA_LIGHT_FRAME:-true}"
OUT_ROOT="${ALPACA_OUT_DIR:-test-results}"
RUN_ID="$(date -u +%Y%m%dT%H%M%SZ)"
OUT_DIR="${OUT_ROOT}/${RUN_ID}"
BASE_URL="http://${HOST}/api/v1/${DEVICE}/${DEVICE_NUMBER}"
SWITCH_BASE_URL="http://${HOST}/api/v1/switch/${ALPACA_SWITCH_NUMBER:-0}"
MGMT_URL="http://${HOST}/management/v1"
TXN=1

mkdir -p "${OUT_DIR}"

log() {
  printf '\n[%s] %s\n' "$(date -u +%H:%M:%S)" "$*"
}

next_txn() {
  TXN=$((TXN + 1))
  printf '%s' "${TXN}"
}

curl_get() {
  local label="$1"
  local url="$2"
  local txn
  txn="$(next_txn)"
  local separator="?"
  if [[ "${url}" == *\?* ]]; then
    separator="&"
  fi
  local full_url="${url}${separator}ClientID=${CLIENT_ID}&ClientTransactionID=${txn}"

  log "GET ${label}"
  printf 'curl --fail --show-error --silent "%s"\n' "${full_url}" | tee -a "${OUT_DIR}/commands.log"
  curl --fail --show-error --silent "${full_url}" | tee "${OUT_DIR}/${label}.json" >/dev/null
}

curl_put() {
  local label="$1"
  local url="$2"
  shift 2
  local txn
  txn="$(next_txn)"
  local args=(-d "ClientID=${CLIENT_ID}" -d "ClientTransactionID=${txn}")

  for param in "$@"; do
    args+=(-d "${param}")
  done

  log "PUT ${label}"
  {
    printf 'curl --fail --show-error --silent --request PUT'
    printf ' --data-urlencode %q' "ClientID=${CLIENT_ID}" "ClientTransactionID=${txn}" "$@"
    printf ' "%s"\n' "${url}"
  } | tee -a "${OUT_DIR}/commands.log"

  curl --fail --show-error --silent --request PUT "${args[@]}" "${url}" | tee "${OUT_DIR}/${label}.json" >/dev/null
}

curl_management_get() {
  local label="$1"
  local url="$2"

  log "GET ${label}"
  printf 'curl --fail --show-error --silent "%s"\n' "${url}" | tee -a "${OUT_DIR}/commands.log"
  curl --fail --show-error --silent "${url}" | tee "${OUT_DIR}/${label}.json" >/dev/null
}

json_value_bool() {
  python3 - "$1" <<'PY'
import json
import sys

with open(sys.argv[1], "r", encoding="utf-8") as fh:
    doc = json.load(fh)
print("true" if doc.get("Value") is True else "false")
PY
}

convert_imagearray() {
  local image_json="$1"
  local basename="$2"

  log "Convert ${image_json} to thermal image files"
  python3 - "${image_json}" "${OUT_DIR}/${basename}" <<'PY'
import json
import math
import shutil
import subprocess
import sys
from pathlib import Path

json_path = Path(sys.argv[1])
out_base = Path(sys.argv[2])

with json_path.open("r", encoding="utf-8") as fh:
    doc = json.load(fh)

pixels = doc.get("Value")
if not pixels:
    raise SystemExit(f"No image Value found in {json_path}")

height = len(pixels)
width = len(pixels[0]) if height else 0
flat = [int(v) for row in pixels for v in row]
mn = min(flat)
mx = max(flat)
span = max(mx - mn, 1)

def colour(value):
    # Four-stop thermal palette: black -> blue -> amber -> white.
    t = (value - mn) / span
    if t < 0.33:
        u = t / 0.33
        return (0, int(40 * u), int(160 + 95 * u))
    if t < 0.72:
        u = (t - 0.33) / 0.39
        return (int(255 * u), int(40 + 120 * u), int(255 * (1 - u)))
    u = (t - 0.72) / 0.28
    return (255, int(160 + 95 * u), int(30 + 225 * u))

ppm_path = out_base.with_suffix(".ppm")
pgm_path = out_base.with_suffix(".pgm")
jpg_path = out_base.with_suffix(".jpg")
meta_path = out_base.with_suffix(".txt")

with ppm_path.open("wb") as fh:
    fh.write(f"P6\n{width} {height}\n255\n".encode("ascii"))
    for value in flat:
        fh.write(bytes(colour(value)))

with pgm_path.open("wb") as fh:
    fh.write(f"P5\n{width} {height}\n255\n".encode("ascii"))
    for value in flat:
        fh.write(bytes([round((value - mn) * 255 / span)]))

with meta_path.open("w", encoding="utf-8") as fh:
    fh.write(f"width={width}\n")
    fh.write(f"height={height}\n")
    fh.write(f"min_c={mn / 100:.2f}\n")
    fh.write(f"max_c={mx / 100:.2f}\n")
    fh.write(f"mean_c={sum(flat) / len(flat) / 100:.2f}\n")

converted = False
if shutil.which("magick"):
    subprocess.run(["magick", str(ppm_path), str(jpg_path)], check=True)
    converted = True
elif shutil.which("convert"):
    subprocess.run(["convert", str(ppm_path), str(jpg_path)], check=True)
    converted = True
elif shutil.which("cjpeg"):
    with jpg_path.open("wb") as out:
        subprocess.run(["cjpeg", str(ppm_path)], check=True, stdout=out)
    converted = True

print(f"Wrote {ppm_path}")
print(f"Wrote {pgm_path}")
print(f"Wrote {meta_path}")
if converted:
    print(f"Wrote {jpg_path}")
else:
    print("JPEG conversion skipped: install ImageMagick 'magick'/'convert' or 'cjpeg'.")
PY
}

wait_for_image() {
  local attempts="${ALPACA_READY_ATTEMPTS:-20}"
  local delay="${ALPACA_READY_DELAY:-0.5}"

  for attempt in $(seq 1 "${attempts}"); do
    curl_get "imageready_${attempt}" "${BASE_URL}/imageready"
    if [[ "$(json_value_bool "${OUT_DIR}/imageready_${attempt}.json")" == "true" ]]; then
      log "Image ready after ${attempt} poll(s)"
      return 0
    fi
    sleep "${delay}"
  done

  log "Image did not report ready after ${attempts} poll(s)"
  return 1
}

log "Writing results to ${OUT_DIR}"
log "Target Alpaca camera: ${BASE_URL}"
log "Target Alpaca sensor switch: ${SWITCH_BASE_URL}"

log "Segment 1: discovery and initial setup"
curl_management_get "configureddevices" "${MGMT_URL}/configureddevices"
curl_get "description" "${BASE_URL}/description"
curl_get "driverinfo" "${BASE_URL}/driverinfo"
curl_get "interfaceversion" "${BASE_URL}/interfaceversion"
curl_get "supportedactions" "${BASE_URL}/supportedactions"
curl_put "connect" "${BASE_URL}/connected" "Connected=true"
curl_get "connected" "${BASE_URL}/connected"
curl_put "switch_connect" "${SWITCH_BASE_URL}/connected" "Connected=true"
curl_get "switch_connected" "${SWITCH_BASE_URL}/connected"
curl_get "cameraxsize" "${BASE_URL}/cameraxsize"
curl_get "cameraysize" "${BASE_URL}/cameraysize"
curl_get "sensorname" "${BASE_URL}/sensorname"
curl_put "action_status_initial" "${BASE_URL}/action" "Action=status" "Parameters="

log "Segment 1b: read auxiliary sensor switch metadata"
curl_get "switch_maxswitch" "${SWITCH_BASE_URL}/maxswitch"
for id in 0 1 2; do
  curl_get "switch_${id}_name" "${SWITCH_BASE_URL}/getswitchname?Id=${id}"
  curl_get "switch_${id}_description" "${SWITCH_BASE_URL}/getswitchdescription?Id=${id}"
  curl_get "switch_${id}_canwrite" "${SWITCH_BASE_URL}/canwrite?Id=${id}"
  curl_get "switch_${id}_min" "${SWITCH_BASE_URL}/minswitchvalue?Id=${id}"
  curl_get "switch_${id}_max" "${SWITCH_BASE_URL}/maxswitchvalue?Id=${id}"
  curl_get "switch_${id}_step" "${SWITCH_BASE_URL}/switchstep?Id=${id}"
done
curl_put "switch_action_status_initial" "${SWITCH_BASE_URL}/action" "Action=status" "Parameters="

log "Segment 2: take an image"
curl_put "startexposure" "${BASE_URL}/startexposure" "Duration=${DURATION}" "Light=${LIGHT}"
wait_for_image
curl_get "camerastate" "${BASE_URL}/camerastate"
curl_get "lastexposureduration" "${BASE_URL}/lastexposureduration"
curl_get "imagearray" "${BASE_URL}/imagearray"
curl_put "action_temperaturemap" "${BASE_URL}/action" "Action=temperaturemap" "Parameters="
curl_put "action_status_after_image" "${BASE_URL}/action" "Action=status" "Parameters="
for id in 0 1 2; do
  curl_get "switch_${id}_value_after_image" "${SWITCH_BASE_URL}/getswitchvalue?Id=${id}"
done
curl_put "switch_action_status_after_image" "${SWITCH_BASE_URL}/action" "Action=status" "Parameters="
convert_imagearray "${OUT_DIR}/imagearray.json" "thermal_image"

log "Segment 3: release camera"
curl_put "switch_disconnect" "${SWITCH_BASE_URL}/connected" "Connected=false"
curl_get "switch_connected_after_disconnect" "${SWITCH_BASE_URL}/connected"
curl_put "disconnect" "${BASE_URL}/connected" "Connected=false"
curl_get "connected_after_disconnect" "${BASE_URL}/connected"

log "Done. Results are in ${OUT_DIR}"
