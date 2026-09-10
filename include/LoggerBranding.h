#pragma once
#include <Arduino.h>

// Matches app/static/apexilabs-mark.svg and the app's brand/Inter styling.
// Embedded mark works offline; Inter falls back to system fonts without WAN.
static const char kLoggerBrandHead[] = R"BRAND(
<link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700;800&display=swap" rel="stylesheet">
<style>
body{font-family:Inter,ui-sans-serif,system-ui,-apple-system,"Segoe UI",sans-serif}
.brand{display:inline-flex;align-items:center;gap:10px;text-decoration:none;color:#ecf2f8;font-weight:700;letter-spacing:.2px;font-size:1.4rem}
.brand svg{width:26px;height:26px;flex-shrink:0}.brand-text em{color:#60a5fa;font-style:normal}
.brand:focus-visible{outline:2px solid #6dd6ff;outline-offset:4px}
.page-label{display:block;margin-top:6px;color:#95a8ba;font-size:.88rem;font-weight:500}
.actions{flex-wrap:wrap}h1{line-height:1.3}input,select,button{font-family:inherit}
</style>
)BRAND";
static const char kLoggerBrandMark[] = R"BRAND(<a class="brand" href="/" aria-label="ApexiLabs Logger home"><svg viewBox="0 0 256 256" aria-hidden="true"><defs><linearGradient id="apx-g" x1="36" y1="220" x2="220" y2="36" gradientUnits="userSpaceOnUse"><stop offset="0" stop-color="#3B82F6"/><stop offset="0.55" stop-color="#60A5FA"/><stop offset="1" stop-color="#22D3EE"/></linearGradient><linearGradient id="apx-h" x1="88" y1="210" x2="170" y2="70" gradientUnits="userSpaceOnUse"><stop offset="0" stop-color="#FFFFFF" stop-opacity="0"/><stop offset="1" stop-color="#FFFFFF" stop-opacity="0.12"/></linearGradient></defs><path d="M128 28 L232 216 H24 Z" fill="url(#apx-g)"/><path d="M128 58 L205 200 H51 Z" fill="#0B1220" opacity="0.88"/><path d="M128 28 L232 216 H150 Z" fill="url(#apx-h)"/></svg><span class="brand-text">ApexiLabs <em>Logger</em></span></a>)BRAND";

inline String loggerBranding(String html) {
  html.replace("</style>",String("</style>")+kLoggerBrandHead);
  html.replace("<h1>Apexi Logger</h1>",String("<h1>")+kLoggerBrandMark+"</h1>");
  html.replace("<h1>Apexi Logger Diagnostics</h1>",String("<h1>")+kLoggerBrandMark+"<span class=\"page-label\">Diagnostics</span></h1>");
  html.replace("<h1>Apexi Logger settings</h1>",String("<h1>")+kLoggerBrandMark+"<span class=\"page-label\">Settings</span></h1>");
  html.replace("<h1>System logs</h1>",String("<h1>")+kLoggerBrandMark+"<span class=\"page-label\">System logs</span></h1>");
  html.replace("<title>Apexi Logger","<title>ApexiLabs Logger");
  html.replace("<title>System logs</title>","<title>ApexiLabs Logger System logs</title>");
  return html;
}
