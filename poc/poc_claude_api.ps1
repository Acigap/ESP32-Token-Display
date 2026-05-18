<#
.SYNOPSIS
  POC: Claude (Anthropic) API - Rate Limit & Time-Based Data
  ทดสอบดึงข้อมูลจาก Anthropic API เพื่อดูว่าได้ข้อมูลอะไรบ้าง

.USAGE
  # วิธีที่ 1: ใส่ key ใน environment variable
  $env:ANTHROPIC_API_KEY = "sk-ant-..."
  .\poc_claude_api.ps1

  # วิธีที่ 2: ใส่ key ตรงๆ ใน script (ไม่แนะนำ commit)
  .\poc_claude_api.ps1 -ApiKey "sk-ant-..."
#>

param(
    [string]$ApiKey = $env:ANTHROPIC_API_KEY
)

# ── config ────────────────────────────────────────────────
$BASE_URL    = "https://api.anthropic.com"
$API_VERSION = "2023-06-01"
$MODEL       = "claude-haiku-4-5"   # cheapest/fastest

# ── helpers ───────────────────────────────────────────────
function Write-Section($title) {
    Write-Host ""
    Write-Host ("=" * 60) -ForegroundColor Cyan
    Write-Host "  $title" -ForegroundColor Cyan
    Write-Host ("=" * 60) -ForegroundColor Cyan
}

function Get-TimeUntil($isoStr) {
    if (-not $isoStr -or $isoStr -eq "N/A") { return "N/A" }
    try {
        $resetDt = [DateTimeOffset]::Parse($isoStr)
        $now     = [DateTimeOffset]::UtcNow
        $diff    = ($resetDt - $now).TotalSeconds
        if ($diff -le 0) { return "already reset" }
        $m = [int]($diff / 60)
        $s = [int]($diff % 60)
        if ($m -gt 0) { return "${m}m ${s}s" } else { return "${s}s" }
    } catch {
        return "parse error"
    }
}

function Get-HeaderVal($headers, $name) {
    # WebResponseObject headers เป็น IDictionary
    if ($headers.ContainsKey($name)) { return $headers[$name] }
    # ลอง case-insensitive
    foreach ($k in $headers.Keys) {
        if ($k -ieq $name) { return $headers[$k] }
    }
    return "N/A"
}

# ── ตรวจ API Key ─────────────────────────────────────────
if (-not $ApiKey) {
    $ApiKey = Read-Host "กรุณาใส่ Anthropic API Key (sk-ant-...)"
}
if (-not $ApiKey) {
    Write-Host "ไม่ได้ใส่ API Key — ยกเลิก" -ForegroundColor Red
    exit 1
}
Write-Host "`nKey prefix: $($ApiKey.Substring(0,[Math]::Min(12,$ApiKey.Length)))... (length=$($ApiKey.Length))"

# ════════════════════════════════════════════════════════
#  TEST 1: Minimal /v1/messages (ดึง rate limit headers)
# ════════════════════════════════════════════════════════
Write-Section "TEST 1: POST /v1/messages (model=$MODEL, max_tokens=1)"

$headers = @{
    "x-api-key"         = $ApiKey
    "anthropic-version" = $API_VERSION
    "content-type"      = "application/json"
}
$body = @{
    model      = $MODEL
    max_tokens = 1
    messages   = @(@{ role = "user"; content = "Hi" })
} | ConvertTo-Json -Depth 3

$response = $null
try {
    $response = Invoke-WebRequest -Uri "$BASE_URL/v1/messages" `
        -Method POST -Headers $headers -Body $body `
        -UseBasicParsing -ErrorAction Stop
} catch {
    # อาจได้ error response ที่มี body (เช่น 429, 400)
    $response = $_.Exception.Response
    if ($null -eq $response) {
        Write-Host "Network error: $_" -ForegroundColor Red
        exit 1
    }
    # อ่าน body จาก error response
    $stream = $response.GetResponseStream()
    $reader = [System.IO.StreamReader]::new($stream)
    $errorBody = $reader.ReadToEnd()
    Write-Host "Status: $([int]$response.StatusCode) $($response.StatusDescription)" -ForegroundColor Yellow
    Write-Host "Error body: $errorBody" -ForegroundColor Yellow
    # ดึง headers จาก error response ด้วย
    $respHeaders = @{}
    foreach ($k in $response.Headers.AllKeys) {
        $respHeaders[$k] = $response.Headers[$k]
    }
    # แสดง rate limit headers
    Write-Host "`n[Rate Limit Headers from error response]" -ForegroundColor Yellow
    foreach ($k in $respHeaders.Keys) {
        if ($k -imatch "ratelimit|retry") {
            Write-Host "  $k : $($respHeaders[$k])" -ForegroundColor Green
        }
    }
    exit 0
}

$statusCode = $response.StatusCode
Write-Host "Status: $statusCode" -ForegroundColor Green

# ── แสดง rate limit headers ทั้งหมด ───────────────────
Write-Host "`n[Rate Limit Headers]" -ForegroundColor Yellow
$rlHeaders = @{}
foreach ($k in $response.Headers.Keys) {
    if ($k -imatch "ratelimit|retry") {
        $rlHeaders[$k] = $response.Headers[$k]
        Write-Host "  $k : $($response.Headers[$k])" -ForegroundColor Green
    }
}
if ($rlHeaders.Count -eq 0) {
    Write-Host "  (ไม่พบ rate limit headers)" -ForegroundColor Gray
}

# ── แสดง response body usage ──────────────────────────
Write-Host "`n[Response Body]" -ForegroundColor Yellow
$bodyObj = $response.Content | ConvertFrom-Json
$usage = $bodyObj.usage
Write-Host "  Model:          $($bodyObj.model)"
Write-Host "  Input tokens:   $($usage.input_tokens)"
Write-Host "  Output tokens:  $($usage.output_tokens)"
Write-Host "  Cache creation: $($usage.cache_creation_input_tokens)"
Write-Host "  Cache read:     $($usage.cache_read_input_tokens)"

# ── parse ─────────────────────────────────────────────
$h = $response.Headers
$data = [ordered]@{
    requests_limit        = Get-HeaderVal $h "anthropic-ratelimit-requests-limit"
    requests_remaining    = Get-HeaderVal $h "anthropic-ratelimit-requests-remaining"
    requests_reset        = Get-HeaderVal $h "anthropic-ratelimit-requests-reset"
    tokens_limit          = Get-HeaderVal $h "anthropic-ratelimit-tokens-limit"
    tokens_remaining      = Get-HeaderVal $h "anthropic-ratelimit-tokens-remaining"
    tokens_reset          = Get-HeaderVal $h "anthropic-ratelimit-tokens-reset"
    input_tokens_limit    = Get-HeaderVal $h "anthropic-ratelimit-input-tokens-limit"
    input_tokens_remaining= Get-HeaderVal $h "anthropic-ratelimit-input-tokens-remaining"
    input_tokens_reset    = Get-HeaderVal $h "anthropic-ratelimit-input-tokens-reset"
    output_tokens_limit   = Get-HeaderVal $h "anthropic-ratelimit-output-tokens-limit"
    output_tokens_remaining= Get-HeaderVal $h "anthropic-ratelimit-output-tokens-remaining"
    output_tokens_reset   = Get-HeaderVal $h "anthropic-ratelimit-output-tokens-reset"
    retry_after           = Get-HeaderVal $h "retry-after"
}

# ════════════════════════════════════════════════════════
#  TEST 2: GET /v1/usage (ถ้ามี — บาง tier เปิดให้)
# ════════════════════════════════════════════════════════
Write-Section "TEST 2: GET usage/billing endpoints"

$getHeaders = @{
    "x-api-key"         = $ApiKey
    "anthropic-version" = $API_VERSION
}
$endpoints = @(
    "/v1/usage",
    "/v1/account",
    "/v1/account/usage"
)
foreach ($ep in $endpoints) {
    try {
        $r = Invoke-WebRequest -Uri "$BASE_URL$ep" -Method GET `
             -Headers $getHeaders -UseBasicParsing -ErrorAction Stop
        Write-Host "  $ep → $($r.StatusCode)" -ForegroundColor Green
        Write-Host "  $($r.Content.Substring(0,[Math]::Min(200,$r.Content.Length)))" -ForegroundColor Gray
    } catch {
        $code = $_.Exception.Response.StatusCode.value__
        Write-Host "  $ep → $code (not available for this key/tier)" -ForegroundColor Gray
    }
}

# ════════════════════════════════════════════════════════
#  SUMMARY
# ════════════════════════════════════════════════════════
Write-Section "SUMMARY: ข้อมูลที่แสดงบน ESP32 ได้"

$fields = [ordered]@{
    "Requests Limit /min"         = $data.requests_limit
    "Requests Remaining"          = $data.requests_remaining
    "Requests Reset in"           = Get-TimeUntil $data.requests_reset
    "Requests Reset (UTC)"        = $data.requests_reset
    "Tokens Limit /min"           = $data.tokens_limit
    "Tokens Remaining"            = $data.tokens_remaining
    "Tokens Reset in"             = Get-TimeUntil $data.tokens_reset
    "Input Tokens Limit"          = $data.input_tokens_limit
    "Input Tokens Remaining"      = $data.input_tokens_remaining
    "Input Tokens Reset in"       = Get-TimeUntil $data.input_tokens_reset
    "Output Tokens Limit"         = $data.output_tokens_limit
    "Output Tokens Remaining"     = $data.output_tokens_remaining
    "Output Tokens Reset in"      = Get-TimeUntil $data.output_tokens_reset
    "Retry-After (if 429)"        = $data.retry_after
}

foreach ($kv in $fields.GetEnumerator()) {
    $v = $kv.Value
    $avail = ($v -ne "N/A") -and ($v -ne "") -and ($null -ne $v)
    $icon  = if ($avail) { "+" } else { "-" }
    $color = if ($avail) { "Green" } else { "Gray" }
    Write-Host ("  {0} {1,-35}: {2}" -f $icon, $kv.Key, $v) -ForegroundColor $color
}

Write-Host ""
Write-Host "[แนะนำสำหรับ ESP32 Display]" -ForegroundColor Cyan
Write-Host "  tokens_remaining   -> bar + number แสดง token ที่เหลือในรอบ"
Write-Host "  tokens_reset_in    -> countdown นับถอยหลังจนถึง reset (แสดงเป็น mm:ss)"
Write-Host "  requests_remaining -> จำนวน request ที่เหลือ"
Write-Host "  ทั้งหมดอยู่ใน Response HEADERS — ไม่ต้อง extra request แยก"
Write-Host ""
Write-Host "Done." -ForegroundColor Green
