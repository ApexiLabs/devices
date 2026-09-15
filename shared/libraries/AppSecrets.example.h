#pragma once

#define APEXI_WIFI_STATION_SSID "replace-me"
#define APEXI_WIFI_STATION_PASSWORD "replace-me"
#define APEXI_OTA_PASSWORD "replace-with-a-strong-unique-password"
#define APEXI_MQTT_HOST "apexlabs-dev"
#define APEXI_MQTT_USERNAME "mda-logger"
#define APEXI_MQTT_PASSWORD "replace-me"

// Use authenticated HTTPS through Cloudflare Access when direct MQTT is not
// reachable. Keep all three credentials unique and out of version control.
#define APEXI_HTTPS_UPLOAD_ENABLED 0
#define APEXI_HTTPS_HOST "app-dev.apexilabs.com"
#define APEXI_HTTPS_PATH "/api/v1/device/loggers/ingest"
#define APEXI_CF_ACCESS_CLIENT_ID "replace-me.access"
#define APEXI_CF_ACCESS_CLIENT_SECRET "replace-me"
#define APEXI_APP_DEVICE_TOKEN "replace-me"
