# SB03 Customer Snore App

This example is the customer-facing ESP32-P4 project. It contains board I/O,
audio capture, NVS mode selection, license console, Dreame logging, TTL output,
and RS485 protocol glue.

It intentionally does not contain model files, YAMNet code, TensorFlow Lite
Micro resolver code, or snore post-processing implementation. The only snore
algorithm dependency is:

```text
components/sb03_snore_prebuilt/lib/libsb03_snore_lib.a
components/sb03_snore_prebuilt/include/sb03_snore_lib.h
```

## Build

```powershell
. "C:\Espressif\tools\Microsoft.v6.0.1.PowerShell_profile.ps1" | Out-Null
idf.py build
```

## Public Library ABI

The application calls only two algorithm functions:

```c
sb03_snore_verify_license(license);
sb03_snore_infer(samples, sample_count, timestamp_ms, &result);
```

`sb03_snore_infer()` accepts one 16 kHz mono PCM frame and returns the common
result struct used by Dreame logs, TTL output, and RS485 responses.

## Serial Commands

The firmware console supports:

```text
device_id get
license set <SB03-...> reboot
comm_mode get
comm_mode set dreame reboot
comm_mode set rs485 reboot
```

The device ID is the ESP32-P4 eFuse MAC as lowercase hex. License strings are
stored in NVS namespace `sb03_lic`, key `license`.
