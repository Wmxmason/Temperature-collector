$ErrorActionPreference = 'Stop'

$project_root = Split-Path -Parent $PSScriptRoot
$test_output = Join-Path $env:TEMP 'rf_collector_frame_test.exe'
$ring_test_output = Join-Path $env:TEMP 'rf_collector_ring_buffer_test.exe'

$required_files = @(
    'Bsp\Inc\collector_app.h',
    'Bsp\Inc\collector_ring_buffer.h',
    'Bsp\Inc\rs485.h',
    'Bsp\Src\collector_app.c',
    'Bsp\Src\collector_ring_buffer.c',
    'Bsp\Src\rs485.c',
    'Core\Inc\rf_receiver.h',
    'Core\Src\rf_receiver.c',
    'Driver\Inc\smartrf_settings.h',
    'Driver\Src\smartrf_settings.c',
    'System\Config\CC1310_LAUNCHXL_TIRTOS.cmd',
    'System\Config\ccfg.c',
    'System\Inc\Board.h',
    'System\Inc\CC1310_LAUNCHXL.h',
    'System\Inc\system_config.h',
    'System\Src\CC1310_LAUNCHXL.c',
    'System\Src\CC1310_LAUNCHXL_fxns.c',
    'Tasks\Src\main_tirtos.c'
)

foreach ($relative_path in $required_files)
{
    if (-not (Test-Path -LiteralPath (Join-Path $project_root $relative_path) -PathType Leaf))
    {
        throw "required architecture file is missing: $relative_path"
    }
}

if (Test-Path -LiteralPath (Join-Path $project_root 'App'))
{
    throw 'unexpected App layer exists'
}

& 'C:\mingw64\bin\gcc.exe' `
    '-std=c99' `
    '-Wall' `
    '-Wextra' `
    '-Werror' `
    "-I$(Join-Path $project_root 'Bsp\Inc')" `
    "-I$(Join-Path $project_root 'System\Inc')" `
    (Join-Path $project_root 'Bsp\Src\collector_frame.c') `
    (Join-Path $PSScriptRoot 'test_collector_frame.c') `
    '-o' $test_output

if ($LASTEXITCODE -ne 0)
{
    throw 'collector_frame unit test build failed'
}

& $test_output
if ($LASTEXITCODE -ne 0)
{
    throw 'collector_frame unit test failed'
}

& 'C:\mingw64\bin\gcc.exe' `
    '-std=c99' `
    '-Wall' `
    '-Wextra' `
    '-Werror' `
    "-I$(Join-Path $project_root 'Bsp\Inc')" `
    (Join-Path $project_root 'Bsp\Src\collector_ring_buffer.c') `
    (Join-Path $PSScriptRoot 'test_collector_ring_buffer.c') `
    '-o' $ring_test_output

if ($LASTEXITCODE -ne 0)
{
    throw 'collector_ring_buffer unit test build failed'
}

& $ring_test_output
if ($LASTEXITCODE -ne 0)
{
    throw 'collector_ring_buffer unit test failed'
}

Write-Output 'COLLECTOR_FRAME_TESTS_OK'
Write-Output 'COLLECTOR_RING_BUFFER_TESTS_OK'
Write-Output 'PROJECT_LAYOUT_TESTS_OK'
