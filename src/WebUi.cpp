#include "WebUi.h"
#include "SystemLog.h"
#include "LoggerBranding.h"

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif

bool WebUi::begin(const AppConfig::WifiConfig &config,
                  CsvLogger &logger,
                  RuntimeSettings &settings,
                  const char *deviceHostname,
                  const char *settingsPassword,
                  bool localSettingsEnabled) {
  logger_ = &logger;
  settings_ = &settings;
  deviceHostname_ = deviceHostname;
  settingsPassword_ = settingsPassword;
  localSettingsEnabled_ = localSettingsEnabled;

  if (config.mode == AppConfig::WifiMode::Station && strlen(config.stationSsid) > 0) {
    WiFi.persistent(false);
#if defined(ESP8266)
    WiFi.hostname(deviceHostname_.c_str());
#else
    WiFi.setHostname(deviceHostname_.c_str());
#endif
    WiFi.mode(WIFI_STA);
    Serial.print("STA joining ");
    Serial.println(config.stationSsid);
    WiFi.begin(config.stationSsid, config.stationPassword);
    const uint32_t startMs = millis();
    while (WiFi.status() != WL_CONNECTED &&
           (millis() - startMs) < (config.connectTimeoutSeconds * 1000UL)) {
      delay(100);
    }
    ready_ = WiFi.status() == WL_CONNECTED;

    if (ready_) {
      mode_ = "STA";
      ipAddress_ = WiFi.localIP().toString();
      Serial.print("STA connected ip=");
      Serial.println(ipAddress_);
    } else {
      Serial.print("STA failed status=");
      Serial.println(static_cast<int>(WiFi.status()));
    }
  }

  if (!ready_ && !config.fallbackApEnabled) {
    mode_ = "OFF";
    ipAddress_ = "0.0.0.0";
    return false;
  }

  if (!ready_) {
    WiFi.persistent(false);
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(100);
    WiFi.mode(WIFI_AP);
#if defined(ESP8266)
    WiFi.setSleepMode(WIFI_NONE_SLEEP);
#else
    WiFi.setSleep(false);
#endif

    const IPAddress apIp(config.apAddress[0], config.apAddress[1], config.apAddress[2],
                         config.apAddress[3]);
    WiFi.softAPConfig(apIp, apIp, IPAddress(255, 255, 255, 0));
    const uint8_t channel = config.apChannel == 0 ? 6 : config.apChannel;
    const char *passphrase = strlen(config.apPassword) >= 8 ? config.apPassword : nullptr;
    ready_ = WiFi.softAP(config.apSsid, passphrase, channel, 0, 4);
    delay(150);
    mode_ = "AP";
    ipAddress_ = WiFi.softAPIP().toString();
  }

  if (!ready_) {
    mode_ = "OFF";
    ipAddress_ = "0.0.0.0";
    return false;
  }

  registerRoutes();
  server_.begin();
  return true;
}

void WebUi::handleClient() {
  if (!ready_) return;
  requestStartedMs_=millis();
  server_.handleClient();
  if (restartPending_ && (millis() - restartRequestedMs_) >= 750) {
    ESP.restart();
  }
}

void WebUi::publishState(const AppState &state) { state_ = state; }

bool WebUi::isReady() const { return ready_; }

String WebUi::modeString() const { return mode_; }

String WebUi::ipAddress() const { return ipAddress_; }

void WebUi::setManagementPairingCode(const String &pairingCode,
                                     const uint32_t expiresInSeconds) {
  managementPairingCode_ = pairingCode;
  managementPairingExpiresInSeconds_ = expiresInSeconds;
}

void WebUi::registerRoutes() {
  server_.on("/api/authorization",HTTP_GET,[this](){
    if(!settingsAuthorized())return;
    server_.sendHeader("Cache-Control","no-store");
    if(!authorization_){sendLogged(503,"application/json","{}");return;}
    String body="{\"status\":\""+jsonEscape(authorization_->status())+"\",\"error\":\""+jsonEscape(authorization_->error())+
      "\",\"user_code\":\""+jsonEscape(authorization_->userCode())+"\",\"expires_in\":"+String(authorization_->expiresIn())+"}";
    sendLogged(200,"application/json",body);
  });
  server_.on("/api/authorization/start",HTTP_POST,[this](){
    if (!localSettingsEnabled_) {server_.send(410,"text/plain","Local authorization disabled");return;}
    if(!settingsAuthorized())return;
    server_.sendHeader("Cache-Control","no-store");
    if(!authorization_ || server_.arg("csrf")!=authorization_->csrfToken() || authorization_->csrfToken().isEmpty()) {
      sendLogged(403,"application/json","{\"error\":\"Invalid authorization request\"}");return;
    }
    sendLogged(authorization_->requestAuthorization()?202:409,"application/json","{}");
  });
  server_.on("/api/logging",HTTP_POST,[this]() {
    if(!settingsAuthorized()) return;
    StaticJsonDocument<128> doc;
    if(!server_.hasArg("plain") || server_.arg("plain").length()>128 || deserializeJson(doc,server_.arg("plain")) || !doc["detailed"].is<bool>()) {
      sendLogged(400,"text/plain","Expected a JSON detailed boolean"); return;
    }
    systemLog.detailedRequests(doc["detailed"].as<bool>());
    sendLogged(200,"application/json","{\"ok\":true}");
  });
  server_.on("/api/system-events",HTTP_GET,[this]() {
    if(!settingsAuthorized()) return;
    sendLogged(200,"application/json",systemLog.recentJson());
  });
  server_.on("/api/log-files",HTTP_GET,[this]() {
    if(!settingsAuthorized()) return;
    sendLogged(200,"application/json",systemLog.filesJson());
  });
  server_.on("/system-log-download",HTTP_GET,[this]() {
    if(!settingsAuthorized()) return;
    File f=systemLog.open(server_.arg("name"));
    if(!f) { sendLogged(404,"text/plain","Log unavailable"); return; }
    server_.sendHeader("Cache-Control","no-store");
    server_.streamFile(f,"text/plain"); f.close(); systemLog.add("log_download");
  });
  server_.on("/logs",HTTP_GET,[this]() {
    if(!settingsAuthorized()) return;
    sendLogged(200,"text/html",loggerBranding(R"HTML(<!doctype html><meta name="viewport" content="width=device-width"><title>System logs</title>
<style>body{background:#09151e;color:#e8eef5;font:16px system-ui;max-width:1100px;margin:2rem auto;padding:1rem}a{color:#7dd3fc}pre{white-space:pre-wrap;overflow-wrap:anywhere}select,button{padding:.5rem;margin:.5rem}</style>
<a href="/diagnostics">Diagnostics</a><h1>System logs</h1><p>Logger and Dash events. Times are logger receipt times; boot and uptime preserve device ordering. RAM buffers are lost on power loss.</p>
<label>Source <select id="source"><option value="">All devices</option><option value="logger">Logger</option><option value="dash">Dash</option></select></label>
<label>Severity <select id="severity"><option value="">All</option><option>INFO</option><option>WARN</option><option>ERROR</option></select></label>
<button id="refresh">Refresh</button><button id="detail">Detailed HTTP logging (10 minutes)</button><p id="status"></p><pre id="events"></pre><h2>Files</h2><div id="files"></div>
<script>let rows=[];function render(){events.textContent=rows.filter(e=>(!source.value||e.device.startsWith(source.value))&&(!severity.value||e.severity===severity.value)).map(e=>JSON.stringify(e)).join('\n')}
async function load(){try{const [a,b]=await Promise.all([fetch('/api/system-events'),fetch('/api/log-files')]);if(!a.ok||!b.ok)throw Error('Log access failed');const d=await a.json();rows=d.events;document.getElementById('status').textContent=`SD ${d.sd_ready?'ready':'unavailable'} · ${d.pending} pending · ${d.dropped} dropped`;render();files.replaceChildren();for(const f of await b.json()){const p=document.createElement('p'),a=document.createElement('a');a.href='/system-log-download?name='+encodeURIComponent(f.name);a.textContent=f.name+' ('+f.size+' bytes)';p.append(a);files.append(p)}}catch(e){document.getElementById('status').textContent=e.message}}
refresh.onclick=load;source.onchange=render;severity.onchange=render;detail.onclick=async()=>{const r=await fetch('/api/logging',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({detailed:true})});if(!r.ok){document.getElementById('status').textContent='Could not enable detailed logging';return}detail.textContent='Detailed HTTP logging enabled for 10 minutes';load()};load();</script>)HTML"));
  });
  server_.on("/", HTTP_GET, [this]() { systemLog.add("http_get_dashboard"); handleIndex(); });
  server_.on("/diagnostics", HTTP_GET, [this]() { systemLog.add("http_get_diagnostics"); handleDiagnostics(); });
  server_.on("/api/live", HTTP_GET, [this]() { handleLiveJson(); });
  server_.on("/api/files", HTTP_GET, [this]() { handleFilesJson(); });
  server_.on("/settings", HTTP_GET, [this]() { systemLog.add("http_get_settings"); handleSettings(); });
  server_.on("/settings", HTTP_POST, [this]() { systemLog.add("http_post_settings"); handleSettingsSave(); });
  server_.onNotFound([this]() { handleDownload(); });
}

void WebUi::sendLogged(int status,const char *type,const String &body) {
  server_.send(status,type,body);
  const String path=server_.uri();
  // Names come from the route allowlist, never a user-supplied path or query.
  const char *code=path=="/"?"http_dashboard":path=="/settings"?"http_settings":path=="/diagnostics"?"http_diagnostics":path=="/logs"?"http_logs":"http_api";
  static uint32_t polls=0,last=0;
  if(!systemLog.detailedRequests() && status==200 && path.startsWith("/api/")) {
    ++polls;
    if(uint32_t(millis()-last)<60000) return;
    systemLog.add("http_poll_summary",polls); polls=0; last=millis();
  } else systemLog.http(code,uint8_t(server_.method()),status,millis()-requestStartedMs_);
}

void WebUi::handleIndex() { sendLogged(200, "text/html", indexHtml()); }

void WebUi::handleDiagnostics() {
  sendLogged(200, "text/html", diagnosticsHtml());
}

void WebUi::handleLiveJson() {
  sendLogged(200, "application/json", liveJson());
}

void WebUi::handleFilesJson() {
  if (logger_ == nullptr) {
    sendLogged(503, "application/json", "[]");
    return;
  }
  sendLogged(200, "application/json", logger_->listFilesJson());
}

void WebUi::handleSettings() {
  if (!localSettingsEnabled_) {
    server_.send(410, "text/plain", "Local settings are disabled; use provisioned device management.");
    return;
  }
  if (!settingsAuthorized()) {
    return;
  }
  server_.sendHeader("Cache-Control","no-store");

  const AppConfig::UploadConfig &upload = settings_->uploadConfig();
  const bool httpsUpload = upload.protocol == AppConfig::UploadConfig::Protocol::Https;
  String html = R"rawliteral(<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><link rel="icon" href="data:,"><title>Apexi Logger Settings</title><style>body{font-family:system-ui;max-width:38rem;margin:2rem auto;padding:0 1rem;background:#09131f;color:#ecf2f8}label{display:block;margin:1rem 0}.hint{color:#95a8ba}input{box-sizing:border-box;width:100%;padding:.7rem;margin-top:.3rem}input[type=checkbox]{width:auto}button{padding:.8rem 1.2rem}a{color:#6dd6ff}h2{margin-top:2rem}</style></head><body><h1>Apexi Logger settings</h1><form method="post" action="/settings"><h2>Upstream server</h2>)rawliteral";
  html += httpsUpload
              ? R"rawliteral(<p class="hint">HTTPS through Cloudflare Access. Credentials may be compiled into the firmware or replaced below. Stored values are never returned by this page.</p>)rawliteral"
              : R"rawliteral(<p class="hint">MQTT credentials remain in the local secrets header and are never returned by this page.</p>)rawliteral";
  html += "<p>Protocol: <strong>" + String(httpsUpload ? "HTTPS" : "MQTT") + "</strong></p>";
  if(httpsUpload && authorization_ && authorization_->supported()) {
    html += "<h2>Device authorization</h2><p>Hardware identity: "+htmlEscape(authorization_->hardwareId())+"</p>";
    html += "<p>Status: <strong id=\"authorizationStatus\">"+htmlEscape(authorization_->status())+"</strong></p>";
    html += "<p id=\"authorizationError\">"+htmlEscape(authorization_->error())+"</p><p>Pairing code: <strong id=\"authorizationCode\">"+htmlEscape(authorization_->userCode())+"</strong></p><p id=\"authorizationExpiry\"></p>";
    html += "<button type=\"button\" id=\"authorizeDevice\" data-csrf=\""+htmlEscape(authorization_->csrfToken())+"\">Connect / re-authorize device</button>";
    html += R"rawliteral(<p class="hint">Approve the temporary code in your signed-in app account. This replaces invalid credentials without copying secrets or flashing firmware. Existing ownership must be confirmed in the app. Approval saves the credential and restarts the logger; buffered telemetry is retained.</p><script>
    (()=>{const button=document.getElementById('authorizeDevice');
    async function refreshAuthorization(){try{const r=await fetch('/api/authorization',{cache:'no-store'});if(!r.ok)return;const s=await r.json();document.getElementById('authorizationStatus').textContent=s.status.replaceAll('_',' ');document.getElementById('authorizationCode').textContent=s.user_code||'Not requested';document.getElementById('authorizationExpiry').textContent=s.user_code?'Code expires in '+s.expires_in+' seconds':'';document.getElementById('authorizationError').textContent=s.error||'';}catch{}}
    button.onclick=async()=>{button.disabled=true;try{const r=await fetch('/api/authorization/start',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams({csrf:button.dataset.csrf})});if(!r.ok)document.getElementById('authorizationError').textContent='Authorization request rejected';await refreshAuthorization();}catch{document.getElementById('authorizationError').textContent='Logger unavailable';}finally{button.disabled=false;}};setInterval(refreshAuthorization,2000);})();</script>)rawliteral";
  }
  html += R"rawliteral(<label><input type="checkbox" name="enabled" value="1")rawliteral";
  if (settings_->liveUploadEnabled()) {
    html += " checked";
  }
  html += R"rawliteral(> Enable live upload</label><label><input type="checkbox" name="remote_management" value="1")rawliteral";
  if (settings_->remoteManagementEnabled()) {
    html += " checked";
  }
  html += R"rawliteral(> Allow remote management</label><p class="hint">Optional. Pair and configure this logger from the ApexiLabs app. Live upload must remain enabled; disable this locally at any time to restore outbound-only operation.</p>)rawliteral";
  if ((!httpsUpload || !authorization_ || !authorization_->supported()) && settings_->remoteManagementEnabled() && !managementPairingCode_.isEmpty()) {
    html += "<p>Temporary pairing code: <strong class=\"pairing-code\">" +
            htmlEscape(managementPairingCode_) + "</strong></p>";
    html += "<p class=\"hint\">Enter only this code in the ApexiLabs app. The app identifies this logger automatically. "
            "This code refreshes every 10 minutes; next refresh in <span id=\"pairingCountdown\" data-seconds=\"" +
            String(managementPairingExpiresInSeconds_) + "\">--:--</span>.</p>";
    html += R"rawliteral(<script>(()=>{const el=document.getElementById('pairingCountdown');let remaining=Number(el.dataset.seconds||0);const render=()=>{const minutes=Math.floor(remaining/60);const seconds=remaining%60;el.textContent=String(minutes).padStart(2,'0')+':'+String(seconds).padStart(2,'0');};render();setInterval(()=>{remaining=Math.max(0,remaining-1);render();if(remaining===0)window.location.reload();},1000);})();</script>)rawliteral";
  }
  html += R"rawliteral(<label>Server host<input name="host" required maxlength="63" value=")rawliteral";
  html += htmlEscape(upload.mqttHost);
  html += R"rawliteral("></label><label>)rawliteral";
  html += httpsUpload ? "HTTPS port" : "MQTT port";
  html += R"rawliteral(<input name="port" type="number" min="1" max="65535" required value=")rawliteral";
  html += String(upload.mqttPort);
  html += R"rawliteral("></label>)rawliteral";
  if (httpsUpload) {
    const String clientIdState = settings_->cloudflareAccessClientIdConfigured()
                                     ? "Configured — leave blank to keep"
                                     : "Not configured";
    const String clientSecretState = settings_->cloudflareAccessClientSecretConfigured()
                                         ? "Configured — leave blank to keep"
                                         : "Not configured";
    html += R"rawliteral(<h2>Cloudflare Access</h2><p class="hint">Enter either field only when replacing it. Existing values are write-only and cannot be retrieved from the logger.</p><label>Client ID<input name="cf_access_client_id" type="password" autocomplete="new-password" spellcheck="false" maxlength="127" placeholder=")rawliteral";
    html += htmlEscape(clientIdState);
    html += R"rawliteral("></label><label>Client secret<input name="cf_access_client_secret" type="password" autocomplete="new-password" spellcheck="false" maxlength="127" placeholder=")rawliteral";
    html += htmlEscape(clientSecretState);
    html += R"rawliteral("></label>)rawliteral";
  }
  html += R"rawliteral(<h2>Network time</h2><label>Primary NTP server<input name="ntp_primary" required maxlength="63" value=")rawliteral";
  html += htmlEscape(settings_->ntpPrimary());
  html += R"rawliteral("></label><label>Secondary NTP server<input name="ntp_secondary" required maxlength="63" value=")rawliteral";
  html += htmlEscape(settings_->ntpSecondary());
  html += R"rawliteral("></label><label>Timezone rule<input name="tz_rule" required maxlength="63" value=")rawliteral";
  html += htmlEscape(settings_->timeZoneRule());
  html += R"rawliteral("></label><p class="hint">POSIX format examples: Perth <code>AWST-8</code>, UTC <code>UTC0</code>, Sydney <code>AEST-10AEDT,M10.1.0,M4.1.0/3</code>.</p><label>Timezone label<input name="tz_label" required maxlength="31" value=")rawliteral";
  html += htmlEscape(settings_->timeZoneLabel());
  html += R"rawliteral("></label><button type="submit">Save and restart</button></form><p><a href="/">Back to status</a></p></body></html>)rawliteral";
  sendLogged(200, "text/html", loggerBranding(html));
}

void WebUi::handleSettingsSave() {
  if (!localSettingsEnabled_) {
    server_.send(410, "text/plain", "Local settings are disabled; use provisioned device management.");
    return;
  }
  if (!settingsAuthorized()) {
    return;
  }

  const String host = server_.arg("host");
  const long portValue = server_.arg("port").toInt();
  const bool uploadEnabled = server_.hasArg("enabled");
  const bool remoteManagementEnabled = server_.hasArg("remote_management");
  if (remoteManagementEnabled && !uploadEnabled) {
    sendLogged(400, "text/plain", "Remote management requires live upload");
    return;
  }
  if (portValue < 1 || portValue > 65535 ||
      !settings_->save(host,
                       static_cast<uint16_t>(portValue),
                       uploadEnabled,
                       server_.arg("ntp_primary"),
                       server_.arg("ntp_secondary"),
                       server_.arg("tz_rule"),
                       server_.arg("tz_label"),
                       remoteManagementEnabled,
                       settings_->appliedConfigVersion(),
                       server_.arg("cf_access_client_id"),
                       server_.arg("cf_access_client_secret"))) {
    sendLogged(400, "text/plain", "Invalid settings");
    return;
  }

  sendLogged(200, "text/html",
               "<!doctype html><meta name=viewport content='width=device-width'><p>Settings saved. The logger is restarting...</p>");
  restartPending_ = true;
  restartRequestedMs_ = millis();
}

bool WebUi::settingsAuthorized() {
  if (settingsPassword_.isEmpty()) {
    sendLogged(503, "text/plain", "Configure APEXI_OTA_PASSWORD before using settings");
    return false;
  }
  if (!server_.authenticate("admin", settingsPassword_.c_str())) {
    server_.requestAuthentication(DIGEST_AUTH, "MDA Logger");
    systemLog.add("http_auth_challenge",401,2);
    return false;
  }
  return true;
}

String WebUi::htmlEscape(const String &value) {
  String escaped;
  escaped.reserve(value.length() + 8);
  for (size_t index = 0; index < value.length(); ++index) {
    switch (value[index]) {
      case '&': escaped += "&amp;"; break;
      case '<': escaped += "&lt;"; break;
      case '>': escaped += "&gt;"; break;
      case '"': escaped += "&quot;"; break;
      case '\'': escaped += "&#39;"; break;
      default: escaped += value[index]; break;
    }
  }
  return escaped;
}

String WebUi::jsonEscape(const String &value) {
  String escaped;
  escaped.reserve(value.length() + 8);
  for (size_t index = 0; index < value.length(); ++index) {
    const char character = value[index];
    switch (character) {
      case '"': escaped += "\\\""; break;
      case '\\': escaped += "\\\\"; break;
      case '\b': escaped += "\\b"; break;
      case '\f': escaped += "\\f"; break;
      case '\n': escaped += "\\n"; break;
      case '\r': escaped += "\\r"; break;
      case '\t': escaped += "\\t"; break;
      default:
        if (static_cast<uint8_t>(character) >= 0x20) {
          escaped += character;
        }
        break;
    }
  }
  return escaped;
}

void WebUi::handleDownload() {
  if (logger_ == nullptr || !server_.uri().startsWith("/download/")) {
    systemLog.add("http_not_found",404,2);
    sendLogged(404, "text/plain", "Not found");
    return;
  }

  const String fileName = server_.uri().substring(strlen("/download/"));
  File file = logger_->openReadOnly(fileName);
  if (!file) {
    sendLogged(404, "text/plain", "File not found");
    return;
  }

  server_.streamFile(file, "text/csv");
  file.close();
}

String WebUi::liveJson() const {
  String json = "{";
  json += "\"timestamp\":\"" + state_.timestamp + "\",";
  json += "\"uptime_ms\":" + String(state_.uptimeMs) + ",";
  json += "\"uptime\":\"" + state_.uptime + "\",";
  json += "\"sensors\":[";
  for (size_t index = 0; index < state_.sensors.size(); ++index) {
    const SensorSnapshot &sensor = state_.sensors[index];
    if (index > 0) {
      json += ",";
    }
    json += "{";
    json += "\"id\":\"" + String(sensor.id) + "\",";
    json += "\"name\":\"" + String(sensor.name) + "\",";
    json += "\"value\":" + String(sensor.filteredValue, 3) + ",";
    json += "\"units\":\"" + String(sensor.units) + "\",";
    json += "\"loop_mA\":" + String(sensor.loopCurrentmA, 3) + ",";
    json += "\"fault\":\"" + String(sensorFaultToString(sensor.activeFault)) + "\"}";
  }
  json += "],";
  json += "\"system\":{";
  json += "\"device_id\":\"" + jsonEscape(state_.system.deviceId) + "\",";
  json += "\"device_name\":\"" + jsonEscape(state_.system.deviceName) + "\",";
  json += "\"hardware_revision\":\"" + jsonEscape(state_.system.hardwareRevision) + "\",";
  json += "\"provisioning_status\":\"" + jsonEscape(state_.system.provisioningStatus) + "\",";
  json += "\"provisioning_error\":\"" + jsonEscape(state_.system.provisioningError) + "\",";
  json += "\"provisioned_at\":\"" + jsonEscape(state_.system.provisionedAt) + "\",";
  json += "\"production_security_required\":" +
          String(state_.system.productionSecurityRequired ? "true" : "false") + ",";
  json += "\"secure_boot_enabled\":" +
          String(state_.system.secureBootEnabled ? "true" : "false") + ",";
  json += "\"flash_encryption_enabled\":" +
          String(state_.system.flashEncryptionEnabled ? "true" : "false") + ",";
  json += "\"flash_encryption_release_mode\":" +
          String(state_.system.flashEncryptionReleaseMode ? "true" : "false") + ",";
  json += "\"production_security_ready\":" +
          String(state_.system.productionSecurityReady ? "true" : "false") + ",";
  json += "\"adc_ready\":" + String(state_.system.adcReady ? "true" : "false") + ",";
  json += "\"display_enabled\":" + String(state_.system.displayEnabled ? "true" : "false") + ",";
  json += "\"rtc_enabled\":" + String(state_.system.rtcEnabled ? "true" : "false") + ",";
  json += "\"rtc_ready\":" + String(state_.system.rtcReady ? "true" : "false") + ",";
  json += "\"rtc_synced\":" + String(state_.system.rtcSynced ? "true" : "false") + ",";
  json += "\"rtc_error\":\"" + jsonEscape(state_.system.rtcError) + "\",";
  json += "\"rtc_last_sync\":\"" + jsonEscape(state_.system.rtcLastSync) + "\",";
  json += "\"time_zone\":\"" + jsonEscape(state_.system.timeZone) + "\",";
  json += "\"sd_enabled\":" + String(state_.system.sdEnabled ? "true" : "false") + ",";
  json += "\"sd_ready\":" + String(state_.system.sdReady ? "true" : "false") + ",";
  json += "\"wifi_ready\":" + String(state_.system.wifiReady ? "true" : "false") + ",";
  json += "\"upload_enabled\":" + String(state_.system.uploadEnabled ? "true" : "false") + ",";
  json += "\"upload_connected\":" + String(state_.system.uploadConnected ? "true" : "false") + ",";
  json += "\"ota_enabled\":" + String(state_.system.otaEnabled ? "true" : "false") + ",";
  json += "\"ota_ready\":" + String(state_.system.otaReady ? "true" : "false") + ",";
  json += "\"wifi_mode\":\"" + state_.system.wifiMode + "\",";
  json += "\"ip_address\":\"" + state_.system.ipAddress + "\",";
  json += "\"current_log_file\":\"" + state_.system.currentLogFile + "\",";
  json += "\"last_log_error\":\"" + jsonEscape(state_.system.lastLogError) + "\",";
  json += "\"upload_protocol\":\"" + state_.system.uploadProtocol + "\",";
  json += "\"upload_server\":\"" + jsonEscape(state_.system.uploadServer) + "\",";
  json += "\"dash_enabled\":" + String(state_.system.dashEnabled ? "true" : "false") + ",";
  json += "\"dash_connected\":" + String(state_.system.dashConnected ? "true" : "false") + ",";
  json += "\"dash_status\":\"" + jsonEscape(state_.system.dashStatus) + "\",";
  json += "\"battery_supported\":" + String(state_.system.batterySupported ? "true" : "false") + ",";
  json += "\"battery_voltage\":" + (state_.system.batteryValid ? String(state_.system.batteryVoltage, 3) : String("null")) + ",";
  json += "\"battery_percent\":" + (state_.system.batteryValid ? String(state_.system.batteryPercent) : String("null")) + ",";
  json += "\"external_power\":" + (state_.system.batterySupported ? String(state_.system.externalPower ? "true" : "false") : String("null")) + ",";
  json += "\"battery_trend\":\"" + jsonEscape(state_.system.batteryTrend) + "\",";
  json += "\"battery_state\":\"" + jsonEscape(state_.system.batteryState) + "\",";
  json += "\"upload_session_id\":\"" + state_.system.uploadSessionId + "\",";
  json += "\"upload_sequence\":" + String(state_.system.lastUploadSequence) + ",";
  json += "\"upload_http_status\":" + String(state_.system.lastUploadHttpStatus) + ",";
  const auto &perf=state_.system.uploadPerformance;
  json += "\"upload_performance\":{\"captured\":"+String(perf.captured)+",\"accepted\":"+String(perf.accepted)+",\"capture_rejected\":"+String(perf.captureRejected)+",\"requests\":"+String(perf.requests)+",\"reused\":"+String(perf.reused)+",\"last_request_ms\":"+String(perf.lastRequestMs)+",\"last_sample_epoch\":"+(perf.lastSampleEpoch?String(perf.lastSampleEpoch):String("null"))+",\"batch_enabled\":"+String(perf.batchEnabled?"true":"false")+",\"batch_requests\":"+String(perf.batchRequests)+",\"batch_accepted\":"+String(perf.batchAccepted)+"},";
  json += "\"upload_evidence_state\":" + String(state_.system.uploadEvidenceState) + ",";
  json += "\"upload_success_age_ms\":" + (state_.system.uploadSuccessAgeMs==UINT32_MAX?String("null"):String(state_.system.uploadSuccessAgeMs)) + ",";
  json += "\"remote_management_enabled\":" +
          String(state_.system.remoteManagementEnabled ? "true" : "false") + ",";
  json += "\"applied_config_version\":" +
          String(state_.system.appliedConfigVersion) + ",";
  json += "\"remote_management_status\":\"" +
          jsonEscape(state_.system.remoteManagementStatus) + "\",";
  json += "\"remote_management_error\":\"" +
          jsonEscape(state_.system.remoteManagementError) + "\",";
  json += "\"store_forward_enabled\":" +
          String(state_.system.storeForwardEnabled ? "true" : "false") + ",";
  json += "\"store_forward_ready\":" +
          String(state_.system.storeForwardReady ? "true" : "false") + ",";
  json += "\"store_forward_pending_records\":" +
          String(state_.system.storeForwardPendingRecords) + ",";
  json += "\"store_forward_pending_bytes\":" +
          String(state_.system.storeForwardPendingBytes) + ",";
  json += "\"store_forward_capacity_bytes\":" +
          String(state_.system.storeForwardCapacityBytes) + ",";
  json += "\"store_forward_dropped_records\":" +
          String(state_.system.storeForwardDroppedRecords) + ",";
  json += "\"store_forward_corruption_events\":" +
          String(state_.system.storeForwardCorruptionEvents) + ",";
  json += "\"store_forward_quarantined_bytes\":" +
          String(state_.system.storeForwardQuarantinedBytes) + ",";
  json += "\"store_forward_error\":\"" + jsonEscape(state_.system.storeForwardError) + "\",";
  json += "\"store_forward_oldest\":" + (state_.system.storeForwardOldestJson.isEmpty() ? String("null") : state_.system.storeForwardOldestJson) + ",";
  json += "\"ota_boot_health\":\"" + jsonEscape(state_.system.otaBootHealth) + "\",";
  json += "\"upload_capture_drops\":" + String(state_.system.uploadCaptureDrops) + ",";
  json += "\"last_upload_error\":\"" + jsonEscape(state_.system.lastUploadError) + "\"}}";
  return json;
}

String WebUi::sensorCardsHtml() const {
  String html;
  for (size_t index = 0; index < AppConfig::kSensorCount; ++index) {
    const AppConfig::SensorConfig &sensor = AppConfig::kSensorConfigs[index];
    html += "<div class=\"card sensor-card\" data-sensor-id=\"" + String(sensor.id) + "\">";
    html += "<div class=\"label\">" + String(sensor.name) + "</div>";
    html += "<div class=\"value\" id=\"sensor-value-" + String(sensor.id) + "\">--</div>";
    html += "<div class=\"status\"><span id=\"sensor-loop-" + String(sensor.id) +
            "\">--</span><span id=\"sensor-fault-" + String(sensor.id) + "\">--</span></div>";
    html += "</div>";
  }
  return html;
}

String WebUi::indexHtml() const {
  const String htmlStart = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="icon" href="data:,">
  <title>Apexi Logger</title>
  <style>
    :root { --bg:#09131f; --surface:#111d2a; --border:#29394a; --text:#ecf2f8; --muted:#95a8ba; --accent:#6dd6ff; --ok:#73d5a2; --warn:#f4c46c; --bad:#ff8d8d; }
    * { box-sizing: border-box; }
    body { margin: 0; font-family: "Segoe UI", system-ui, sans-serif; background: var(--bg); color: var(--text); }
    header { display:flex; align-items:flex-start; justify-content:space-between; gap:16px; padding:18px 20px; background:#102235; border-bottom:1px solid var(--border); }
    h1 { margin: 0; font-size: 1.4rem; }
    .header-meta { margin-top:4px; color:var(--muted); font-size:.88rem; }
    .actions { display:flex; gap:8px; }
    .action-link { flex:none; padding:9px 12px; border:1px solid var(--border); border-radius:8px; color:var(--text); text-decoration:none; }
    main { width:min(100%, 72rem); margin:0 auto; padding:16px; }
    .sensor-grid { display:grid; gap:12px; grid-template-columns:repeat(auto-fit, minmax(180px, 240px)); }
    .support-grid { display:grid; gap:12px; margin-top:12px; grid-template-columns:repeat(2, minmax(0, 1fr)); }
    .card { min-width:0; background:var(--surface); border:1px solid var(--border); border-radius:12px; padding:14px; }
    .sensor-card { min-height:116px; }
    .label { color:var(--muted); font-size:.78rem; font-weight:700; letter-spacing:.08em; text-transform:uppercase; }
    .value { font-size:1.8rem; margin-top:6px; }
    .status { display:flex; align-items:flex-start; justify-content:space-between; gap:16px; margin-top:11px; font-size:.94rem; }
    .status > :last-child { min-width:0; text-align:right; }
    .state { font-size:.78rem; font-weight:700; letter-spacing:.04em; }
    .state.ok { color:var(--ok); }
    .state.warn { color:var(--warn); }
    .state.bad { color:var(--bad); }
    .summary { margin:10px 0 0; color:var(--muted); line-height:1.45; }
    .card-link { display:inline-block; margin-top:12px; }
    .disabled { opacity:.52; }
    .disabled .card-link { color:var(--muted); pointer-events:none; text-decoration:none; }
    ul { margin:10px 0 0; padding-left:18px; }
    li + li { margin-top:8px; }
    a { color:var(--accent); }
    a:focus-visible { outline:2px solid var(--accent); outline-offset:3px; }
    @media (max-width:620px) { header { padding:16px; flex-direction:column; } .actions { flex-direction:row; } main { padding:12px; } .sensor-grid,.support-grid { grid-template-columns:1fr; } }
  </style>
</head>
<body>
  <header>
    <div><h1>Apexi Logger</h1><div class="header-meta" id="stamp">Waiting for data...</div></div>
    <nav class="actions" aria-label="Logger pages"><a class="action-link" href="/diagnostics">Diagnostics</a><a class="action-link" href="/settings">Settings</a></nav>
  </header>
  <main>
    <section class="sensor-grid" aria-label="Sensor readings">
)rawliteral";

  const String htmlEnd = R"rawliteral(
    </section>
    <section class="support-grid" aria-label="Logger status">
      <div class="card" id="healthCard">
        <div class="label">Fault finding</div>
        <div class="status"><span>Logger health</span><span class="state" id="healthStatus">CHECKING</span></div>
        <p class="summary" id="healthSummary">Checking sensors and logger systems...</p>
        <a class="card-link" href="/diagnostics">View full diagnostics</a>
      </div>
      <div class="card" id="csvCard">
      <div class="label">CSV Files</div>
      <p class="summary" id="csvSummary">Checking microSD logging...</p>
      <ul id="fileList"></ul>
      </div>
    </section>
  </main>
  <script>
    let csvFilesEnabled = false;

    function normalUploadQueue(system) {
      return system.upload_enabled && system.upload_connected && system.store_forward_ready &&
        Number.isInteger(system.store_forward_pending_records) && system.store_forward_pending_records >= 0 && system.store_forward_pending_records <= 2 &&
        Number.isFinite(system.upload_success_age_ms) && system.upload_success_age_ms >= 0 && system.upload_success_age_ms <= 10000 &&
        /^Replaying onboard queue: \d+ pending$/.test(system.last_upload_error || '');
    }

    function setState(id, text, tone) {
      const target = document.getElementById(id);
      target.textContent = text;
      target.className = 'state' + (tone ? ' ' + tone : '');
    }

    async function refreshLive() {
      const response = await fetch('/api/live');
      const data = await response.json();
      document.getElementById('stamp').textContent = data.timestamp + ' ' + data.system.time_zone + ' | uptime ' + data.uptime;
      data.sensors.forEach((sensor) => {
        document.getElementById('sensor-value-' + sensor.id).textContent = sensor.value.toFixed(sensor.units === 'bar' ? 2 : 1) + ' ' + sensor.units;
        document.getElementById('sensor-loop-' + sensor.id).textContent = sensor.loop_mA.toFixed(2) + ' mA';
        const fault = document.getElementById('sensor-fault-' + sensor.id);
        fault.textContent = sensor.fault === 'none' ? 'OK' : sensor.fault.toUpperCase();
        fault.className = 'state ' + (sensor.fault === 'none' ? 'ok' : 'bad');
      });
      const issues = [];
      data.sensors.forEach((sensor) => { if (sensor.fault !== 'none') issues.push(sensor.name + ': ' + sensor.fault); });
      if (!data.system.adc_ready) issues.push('ADC is not ready');
      if (data.system.rtc_enabled && !data.system.rtc_ready) issues.push('RTC: ' + (data.system.rtc_error || 'not ready'));
      else if (data.system.rtc_enabled && data.system.rtc_error) issues.push('RTC: ' + data.system.rtc_error);
      if (data.system.sd_enabled && !data.system.sd_ready) issues.push('Logging: ' + (data.system.last_log_error || 'microSD is not ready'));
      else if (data.system.sd_enabled && data.system.last_log_error) issues.push('Logging: ' + data.system.last_log_error);
      if (data.system.upload_enabled && !data.system.upload_connected) issues.push('Upload: ' + (data.system.last_upload_error || 'upstream server is not connected'));
      else if (data.system.last_upload_error && !normalUploadQueue(data.system)) issues.push('Upload: ' + data.system.last_upload_error);
      if (data.system.remote_management_error) issues.push('Remote management: ' + data.system.remote_management_error);
      if (data.system.store_forward_enabled && !data.system.store_forward_ready) issues.push('Queue: ' + (data.system.store_forward_error || 'not ready'));
      else if (data.system.store_forward_error) issues.push('Queue: ' + data.system.store_forward_error);
      if (issues.length) {
        setState('healthStatus', issues.length + ' ISSUE' + (issues.length === 1 ? '' : 'S'), 'bad');
        document.getElementById('healthSummary').textContent = issues.slice(0, 2).join('. ') + (issues.length > 2 ? '. View diagnostics for more.' : '.');
      } else {
        setState('healthStatus', 'NOMINAL', 'ok');
        document.getElementById('healthSummary').textContent = 'Sensors and enabled logger systems are operating normally.';
      }
      const csvCard = document.getElementById('csvCard');
      const csvSummary = document.getElementById('csvSummary');
      if (!data.system.sd_enabled) {
        csvFilesEnabled = false;
        csvCard.classList.add('disabled');
        csvCard.setAttribute('aria-disabled', 'true');
        csvSummary.textContent = 'MicroSD logging is disabled in this firmware.';
        document.getElementById('fileList').innerHTML = '';
      } else if (!data.system.sd_ready) {
        csvFilesEnabled = false;
        csvCard.classList.remove('disabled');
        csvCard.removeAttribute('aria-disabled');
        csvSummary.textContent = 'MicroSD logging is enabled but the card is not ready.';
      } else {
        const shouldLoadFiles = !csvFilesEnabled;
        csvFilesEnabled = true;
        csvCard.classList.remove('disabled');
        csvCard.removeAttribute('aria-disabled');
        csvSummary.textContent = 'Download logs stored on the microSD card.';
        if (shouldLoadFiles) refreshFiles();
      }
    }

    async function refreshFiles() {
      const response = await fetch('/api/files');
      const files = await response.json();
      const target = document.getElementById('fileList');
      target.innerHTML = '';
      if (!files.length) {
        const empty = document.createElement('li');
        empty.textContent = 'No files available';
        target.appendChild(empty);
        return;
      }
      files.forEach((file) => {
        const li = document.createElement('li');
        const a = document.createElement('a');
        a.href = '/download/' + file.name.replace(/^\//, '');
        a.textContent = file.name + ' (' + (file.size / 1000000).toFixed(2) + ' MB)';
        li.appendChild(a);
        target.appendChild(li);
      });
    }

    async function refreshLiveSafely() {
      try {
        await refreshLive();
      } catch (error) {
        document.getElementById('stamp').textContent = 'Web UI refresh failed';
      }
    }

    refreshLiveSafely();
    setInterval(refreshLiveSafely, 1000);
    setInterval(() => { if (csvFilesEnabled) refreshFiles().catch(() => {}); }, 10000);
  </script>
</body>
</html>
)rawliteral";

  return loggerBranding(htmlStart + sensorCardsHtml() + htmlEnd);
}

String WebUi::diagnosticsHtml() const {
  return loggerBranding(R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="icon" href="data:,">
  <title>Apexi Logger Diagnostics</title>
  <style>
    :root { --bg:#09131f; --surface:#111d2a; --border:#29394a; --text:#ecf2f8; --muted:#95a8ba; --accent:#6dd6ff; --ok:#73d5a2; --warn:#f4c46c; --bad:#ff8d8d; }
    * { box-sizing:border-box; }
    body { margin:0; font-family:"Segoe UI",system-ui,sans-serif; background:var(--bg); color:var(--text); }
    header { display:flex; align-items:flex-start; justify-content:space-between; gap:16px; padding:18px 20px; background:#102235; border-bottom:1px solid var(--border); }
    h1 { margin:0; font-size:1.4rem; }
    .header-meta { margin-top:4px; color:var(--muted); font-size:.88rem; }
    .actions { display:flex; gap:8px; }
    .action-link { padding:9px 12px; border:1px solid var(--border); border-radius:8px; color:var(--text); text-decoration:none; }
    main { width:min(100%,72rem); margin:0 auto; padding:16px; }
    .grid { display:grid; gap:12px; grid-template-columns:repeat(2,minmax(0,1fr)); }
    .card { min-width:0; background:var(--surface); border:1px solid var(--border); border-radius:12px; padding:16px; }
    .label { color:var(--muted); font-size:.78rem; font-weight:700; letter-spacing:.08em; text-transform:uppercase; }
    .status { display:flex; align-items:flex-start; justify-content:space-between; gap:16px; margin-top:11px; font-size:.94rem; }
    .status > :last-child { min-width:0; text-align:right; overflow-wrap:anywhere; }
    .detail-row { display:block; padding-top:11px; border-top:1px solid rgba(149,168,186,.16); }
    .detail-row span { display:block; }
    .detail-row span:last-child { margin-top:4px; color:var(--text); text-align:left; }
    .state { font-size:.78rem; font-weight:700; letter-spacing:.04em; }
    .state.ok { color:var(--ok); } .state.warn { color:var(--warn); } .state.bad { color:var(--bad); }
    a { color:var(--accent); } a:focus-visible { outline:2px solid var(--accent); outline-offset:3px; }
    @media(max-width:620px) { header { padding:16px; flex-direction:column; } .actions { flex-direction:row; } main { padding:12px; } .grid { grid-template-columns:1fr; } }
  </style>
</head>
<body>
  <header><div><h1>Apexi Logger Diagnostics</h1><div class="header-meta" id="stamp">Waiting for data...</div></div><nav class="actions" aria-label="Logger pages"><a class="action-link" href="/">Dashboard</a><a class="action-link" href="/logs">System logs</a><a class="action-link" href="/settings">Settings</a></nav></header>
  <main><section class="grid">
    <div class="card"><div class="label">Battery &amp; power</div><div class="status"><span>Estimated charge</span><span id="batteryPercent">--</span></div><div class="status"><span>Battery voltage</span><span id="batteryVoltage">--</span></div><div class="status"><span>USB / 5V power</span><span id="externalPower">--</span></div><div class="status"><span>Voltage trend</span><span id="batteryTrend">--</span></div><div class="status detail-row"><span>Charging indicator</span><span id="batteryState">--</span></div><p>Estimate for a 1S 4.2V LiPo, not a fuel gauge. Charging and load affect accuracy. Trend needs two minutes; voltage cannot confirm charge completion or battery presence.</p></div>
    <div class="card"><div class="label">Apexi Dash</div><div class="status"><span>Bluetooth connection</span><span class="state" id="dashStatus">--</span></div><div class="status"><span>Link status</span><span id="dashDetail">--</span></div></div>
    <div class="card"><div class="label">Connectivity</div><div class="status"><span>Server</span><span class="state" id="uploadStatus">--</span></div><div class="status"><span>Protocol</span><span id="uploadProtocol">--</span></div><div class="status"><span>Wi-Fi</span><span id="wifiStatus">--</span></div><div class="status"><span>Remote management</span><span class="state" id="remoteManagementStatus">--</span></div><div class="status"><span>Applied configuration</span><span id="configVersion">--</span></div><div class="status detail-row"><span>Upstream endpoint</span><span id="uploadServer">--</span></div></div>
    <div class="card"><div class="label">Hardware &amp; time</div><div class="status"><span>ADC</span><span class="state" id="adcStatus">--</span></div><div class="status"><span>RTC</span><span class="state" id="rtcStatus">--</span></div><div class="status"><span>Last time sync</span><span id="rtcLastSync">--</span></div><div class="status"><span>OTA updates</span><span class="state" id="otaStatus">--</span></div></div>
    <div class="card"><div class="label">Storage</div><div class="status"><span>Onboard queue</span><span id="queueStatus">--</span></div><div class="status"><span>Queue capacity</span><span id="queueCapacity">--</span></div><div class="status"><span>Dropped records</span><span id="queueDropped">--</span></div><div class="status"><span>SD logging</span><span class="state" id="sdStatus">--</span></div><div class="status detail-row"><span>Current log file</span><span id="logFile">--</span></div></div>
    <div class="card"><div class="label">Transport</div><div class="status detail-row"><span>Upload session</span><span id="uploadSession">--</span></div><div class="status"><span>Upload sequence</span><span id="uploadSequence">--</span></div><div class="status detail-row"><span>Upload error</span><span id="uploadError">No errors</span></div><div class="status detail-row"><span>Remote-management error</span><span id="remoteManagementError">No errors</span></div></div>
    <div class="card"><div class="label">Hardware errors</div><div class="status detail-row"><span>Queue</span><span id="queueError">No errors</span></div><div class="status detail-row"><span>RTC</span><span id="rtcError">No errors</span></div><div class="status detail-row"><span>Logging</span><span id="logError">No errors</span></div></div>
    <div class="card"><div class="label">Sensors</div><div id="sensorDiagnostics">--</div></div>
  </section></main>
  <script>
    function text(id,value){document.getElementById(id).textContent=value;}
    function state(id,value,tone){const el=document.getElementById(id);el.textContent=value;el.className='state'+(tone?' '+tone:'');}
    function serverHostname(endpoint){
      if(!endpoint) return 'Not configured';
      try { return new URL(endpoint.includes('://')?endpoint:'http://'+endpoint).hostname || 'Not configured'; }
      catch(error) { return 'Invalid server'; }
    }
    async function refresh(){
      const response=await fetch('/api/live'); const data=await response.json();
      text('stamp',data.timestamp+' '+data.system.time_zone+' | uptime '+data.uptime);
      const upload=data.system.upload_enabled?(data.system.upload_connected?'CONNECTED':'WAITING'):'DISABLED'; state('uploadStatus',upload,upload==='CONNECTED'?'ok':(upload==='WAITING'?'warn':''));
      text('uploadProtocol',data.system.upload_protocol.toUpperCase()); text('wifiStatus',data.system.wifi_mode+' '+data.system.ip_address); text('uploadServer',serverHostname(data.system.upload_server));
      const dash=data.system.dash_enabled?(data.system.dash_connected?'CONNECTED':'DISCONNECTED'):'DISABLED'; state('dashStatus',dash,dash==='CONNECTED'?'ok':(dash==='DISCONNECTED'?'warn':'')); text('dashDetail',data.system.dash_status||'--');
      text('batteryPercent',Number.isFinite(data.system.battery_percent)?'~'+data.system.battery_percent+'%':'Unavailable');
      text('batteryVoltage',Number.isFinite(data.system.battery_voltage)?data.system.battery_voltage.toFixed(2)+' V':'--');
      text('externalPower',data.system.battery_supported?(data.system.external_power?'Present':'Absent'):'Unsupported');
      text('batteryTrend',data.system.battery_trend||'--'); text('batteryState',data.system.battery_state||'Unsupported');
      state('remoteManagementStatus',data.system.remote_management_enabled?'ENABLED':'DISABLED',data.system.remote_management_enabled?'ok':''); text('configVersion','v'+data.system.applied_config_version+' '+(data.system.remote_management_status||'ready'));
      state('adcStatus',data.system.adc_ready?'READY':'FAULT',data.system.adc_ready?'ok':'bad');
      const rtc=data.system.rtc_enabled?(data.system.rtc_ready?(data.system.rtc_synced?'NTP SYNCED':'HOLDOVER'):'FAULT'):'DISABLED'; state('rtcStatus',rtc,rtc==='FAULT'?'bad':(rtc==='NTP SYNCED'?'ok':'warn')); text('rtcLastSync',data.system.rtc_last_sync||'--');
      const ota=data.system.ota_enabled?(data.system.ota_ready?'READY':'LOCKED'):'DISABLED'; state('otaStatus',ota,ota==='READY'?'ok':(ota==='LOCKED'?'warn':''));
      text('queueStatus',data.system.store_forward_enabled?(data.system.store_forward_ready?data.system.store_forward_pending_records+' pending / '+Math.round(data.system.store_forward_pending_bytes/1024)+' KiB':'FAULT'):'DISABLED'); text('queueCapacity',Math.round(data.system.store_forward_capacity_bytes/1024)+' KiB'); text('queueDropped',data.system.store_forward_dropped_records);
      const sd=data.system.sd_enabled?(data.system.sd_ready?'READY':'FAULT'):'DISABLED'; state('sdStatus',sd,sd==='READY'?'ok':(sd==='FAULT'?'bad':'')); text('logFile',data.system.current_log_file||'--');
      text('uploadSession',data.system.upload_session_id||'--'); text('uploadSequence',data.system.upload_sequence); text('uploadError',data.system.last_upload_error||'No errors'); text('remoteManagementError',data.system.remote_management_error||'No errors'); text('queueError',data.system.store_forward_error||'No errors'); text('rtcError',data.system.rtc_error||'No errors'); text('logError',data.system.last_log_error||'No errors');
      const sensors=document.getElementById('sensorDiagnostics'); sensors.innerHTML=''; data.sensors.forEach((sensor)=>{const row=document.createElement('div');row.className='status';const name=document.createElement('span');name.textContent=sensor.name+' ('+sensor.loop_mA.toFixed(2)+' mA)';const fault=document.createElement('span');fault.className='state '+(sensor.fault==='none'?'ok':'bad');fault.textContent=sensor.fault==='none'?'OK':sensor.fault.toUpperCase();row.append(name,fault);sensors.appendChild(row);});
    }
    async function refreshSafely(){try{await refresh();}catch(error){text('stamp','Diagnostics refresh failed');}}
    refreshSafely(); setInterval(refreshSafely,1000);
  </script>
</body></html>
)rawliteral");
}
