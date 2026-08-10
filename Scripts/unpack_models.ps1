# Распаковка скачанных моделей: у части папок модель лежит внутри zip.
# Архивы распаковываются рядом, в подпапку unpacked.
$root = "C:\Dev\RageStrike\ImportSource"

Get-ChildItem $root -Recurse -File -Filter *.zip | ForEach-Object {
    $dest = Join-Path $_.DirectoryName "unpacked"
    if (-not (Test-Path $dest)) {
        New-Item -ItemType Directory -Path $dest | Out-Null
    }
    try {
        Expand-Archive -Path $_.FullName -DestinationPath $dest -Force -ErrorAction Stop
        Write-Output ("OK   {0}" -f $_.FullName.Substring($root.Length + 1))
    }
    catch {
        Write-Output ("FAIL {0}: {1}" -f $_.Name, $_.Exception.Message)
    }
}

# вложенные архивы (встречается .zip.zip)
Get-ChildItem $root -Recurse -File -Filter *.zip | Where-Object { $_.DirectoryName -like "*unpacked*" } | ForEach-Object {
    $dest = Join-Path $_.DirectoryName ($_.BaseName + "_x")
    if (-not (Test-Path $dest)) { New-Item -ItemType Directory -Path $dest | Out-Null }
    try {
        Expand-Archive -Path $_.FullName -DestinationPath $dest -Force -ErrorAction Stop
        Write-Output ("OK2  {0}" -f $_.Name)
    }
    catch {
        Write-Output ("FAIL2 {0}" -f $_.Name)
    }
}
