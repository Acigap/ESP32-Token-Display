<#
.SYNOPSIS
  POC: Claude.ai Web Usage Data (Session / Weekly limits)
  ดึงข้อมูล session usage % และ weekly usage % จาก claude.ai
  (ไม่ใช่ Anthropic API - ใช้ session cookie จาก browser แทน)

.HOW TO GET YOUR SESSION KEY
  1. เปิด https://claude.ai ใน Chrome / Edge / Firefox
  2. กด F12 → Application tab → Cookies → https://claude.ai
  3. ค้นหา cookie ชื่อ "sessionKey"  (หรือ __Host-next-auth.session-token)
  4. คัดลอก Value ทั้งหมด แล้ว paste ตอนถาม

.USAGE
  $env:CLAUDEAI_SESSION = "sk-ant-sid01-..."
  .\poc_claudeai_usage.ps1

  # หรือรันแล้วใส่ key ตอน prompt
  .\poc_claudeai_usage.ps1
#>

param(
    [string]$SessionKey = $env:CLAUDEAI_SESSION
)

# ── load .env ─────────────────────────────────────────────
$envFile = Join-Path $PSScriptRoot ".env"
if (Test-Path $envFile) {
    foreach ($line in Get-Content $envFile) {
        $line = $line.Trim()
        if (-not $line -or $line.StartsWith("#") -or $line -notmatch "=") { continue }
        $k, $v = $line -split "=", 2
        $k = $k.Trim(); $v = $v.Trim().Trim('"').Trim("'")
        if ($k -and -not [System.Environment]::GetEnvironmentVariable($k)) {
            [System.Environment]::SetEnvironmentVariable($k, $v, "Process")
        }
    }
    if (-not $SessionKey) { $SessionKey = $env:CLAUDEAI_SESSION }
}

# ── config ────────────────────────────────────────────────
$BASE = "https://claude.ai"

function Write-Section($title) {
    Write-Host ""
    Write-Host ("-" * 60) -ForegroundColor Cyan
    Write-Host "  $title" -ForegroundColor Cyan
    Write-Host ("-" * 60) -ForegroundColor Cyan
}

function Show-Json($obj) {
    $obj | ConvertTo-Json -Depth 10 | Write-Host -ForegroundColor White
}

# ── get session key ───────────────────────────────────────
if (-not $SessionKey) {
    Write-Host ""
    Write-Host "วิธีหา Session Key:" -ForegroundColor Yellow
    Write-Host "  1. เปิด https://claude.ai ใน browser แล้ว login"
    Write-Host "  2. กด F12 > Application > Cookies > https://claude.ai"
    Write-Host "  3. หา cookie ชื่อ 'sessionKey'"
    Write-Host "  4. คัดลอก Value ทั้งหมด"
    Write-Host ""
    $SessionKey = Read-Host "วาง Session Key ที่นี่"
}

if (-not $SessionKey) {
    Write-Host "ไม่มี session key — ออกจากโปรแกรม" -ForegroundColor Red
    exit 1
}

# ── build headers & session ───────────────────────────────
$headers = @{
    "Cookie"     = "sessionKey=$SessionKey"
    "User-Agent" = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"
    "Accept"     = "application/json"
    "Referer"    = "https://claude.ai/"
}

$session = New-Object Microsoft.PowerShell.Commands.WebRequestSession
$cookie  = New-Object System.Net.Cookie("sessionKey", $SessionKey, "/", "claude.ai")
$session.Cookies.Add($cookie)

function Invoke-ClaudeAI($path) {
    try {
        $resp = Invoke-WebRequest -Uri "$BASE$path" `
                                  -Headers $headers `
                                  -WebSession $session `
                                  -UseBasicParsing `
                                  -ErrorAction Stop
        return @{
            Status  = $resp.StatusCode
            Body    = ($resp.Content | ConvertFrom-Json -ErrorAction SilentlyContinue)
            Raw     = $resp.Content
        }
    } catch {
        return @{
            Status  = $_.Exception.Response.StatusCode.value__
            Body    = $null
            Error   = $_.Exception.Message
        }
    }
}

# ══════════════════════════════════════════════════════════
#  STEP 1: Auth / Session info
# ══════════════════════════════════════════════════════════
Write-Section "STEP 1: Session / Auth"
$authEndpoints = @(
    "/api/auth/session",
    "/api/bootstrap",
    "/api/account"
)

$orgId = $null
foreach ($ep in $authEndpoints) {
    Write-Host "`nGET $ep" -ForegroundColor DarkGray
    $r = Invoke-ClaudeAI $ep
    Write-Host "  Status: $($r.Status)"
    if ($r.Body) {
        Show-Json $r.Body
        # try to extract org id
        if (-not $orgId -and $r.Body) {
            $orgId = if ($r.Body.account.memberships) { $r.Body.account.memberships[0].organization.uuid }
                     elseif ($r.Body.organizations)   { $r.Body.organizations[0].uuid }
                     else                             { $r.Body.organization_id }
        }
    } elseif ($r.Error) {
        Write-Host "  Error: $($r.Error)" -ForegroundColor Red
    }
}

# ══════════════════════════════════════════════════════════
#  STEP 2: Organization list (ถ้า session ใช้ได้)
# ══════════════════════════════════════════════════════════
Write-Section "STEP 2: Organizations"
$r = Invoke-ClaudeAI "/api/organizations"
Write-Host "Status: $($r.Status)"
if ($r.Body) {
    Show-Json $r.Body
    if (-not $orgId -and $r.Body -is [array]) {
        $orgId = $r.Body[0].uuid
    }
}

# ══════════════════════════════════════════════════════════
#  STEP 3: Usage limits (ลอง endpoint ที่เป็นไปได้)
# ══════════════════════════════════════════════════════════
Write-Section "STEP 3: Usage Limits"

$usageEndpoints = @(
    "/api/usage_limits",
    "/api/usage",
    "/api/entitlements",
    "/api/rate_limits"
)

if ($orgId) {
    Write-Host "Detected org_id: $orgId" -ForegroundColor Green
    $usageEndpoints += "/api/organizations/$orgId/usage"
    $usageEndpoints += "/api/organizations/$orgId/limits"
    $usageEndpoints += "/api/organizations/$orgId/entitlements"
}

foreach ($ep in $usageEndpoints) {
    Write-Host "`nGET $ep" -ForegroundColor DarkGray
    $r = Invoke-ClaudeAI $ep
    Write-Host "  Status: $($r.Status)"
    if ($r.Body) {
        Show-Json $r.Body
    } elseif ($r.Error) {
        Write-Host "  Error: $($r.Error)" -ForegroundColor DarkGray
    }
}

# ══════════════════════════════════════════════════════════
#  STEP 4: ดักจับ usage จาก bootstrap (ถ้ามี)
# ══════════════════════════════════════════════════════════
Write-Section "STEP 4: Parse Usage Fields"

$r = Invoke-ClaudeAI "/api/bootstrap"
if ($r.Body) {
    # พยายาม extract usage fields ที่เกี่ยวข้อง
    $usageFields = @(
        "limits", "usage", "quotas", "entitlements",
        "session_usage", "weekly_usage", "rate_limits",
        "conversation_limits", "plan"
    )
    Write-Host "Fields found in /api/bootstrap:" -ForegroundColor Yellow
    foreach ($f in $usageFields) {
        $val = $r.Body.$f
        if ($val) {
            Write-Host "  .$f = " -NoNewline -ForegroundColor Green
            Show-Json $val
        }
    }
}

Write-Host ""
Write-Host ("=" * 60) -ForegroundColor Cyan
Write-Host "  POC done" -ForegroundColor Cyan
Write-Host "  Status 401 = session key invalid" -ForegroundColor Yellow
Write-Host "  Status 200 + data = usable on ESP32" -ForegroundColor Yellow
Write-Host ("=" * 60) -ForegroundColor Cyan
