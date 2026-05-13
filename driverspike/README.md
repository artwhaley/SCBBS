# driverspike

This is the canonical source copy of the Spikey UMDF HID driver for the live SCBBS project.

Canonical repository location:

```text
C:\Users\artwh\OneDrive\Documents\SCMFD\SCBBS\driverspike
```

Historical spike location:

```text
C:\Users\artwh\Downloads\Star Commander MFD\driverspike
```

The historical location may still have an installed/running build on the development machine, but new source work should happen here in the SCBBS repository.

## Build

From this directory:

```powershell
.\Build-VS2022.cmd /t:Rebuild /p:Configuration=Debug /p:Platform=x64 /m /v:minimal /nr:true
```

The build writes driver and test-client outputs under:

```text
driverspike\x64\
```

That output folder is intentionally ignored by git.

The default solution builds the driver and `TestEnumeratorAndClient`. The old `KeystrokeInjectorGui` source is retained for historical reference, but it is not part of the default solution because the final SCBBS integration uses the .NET `SpikeyHid` backend instead of the old GUI.

## Deploy

Deploy only from an elevated PowerShell, and only after confirming the signing mode expected for the package being installed.

For the current development rig, use the known cleanup/redeploy rhythm:

```powershell
cd "C:\Users\artwh\OneDrive\Documents\SCMFD\SCBBS\driverspike"
.\TheSpikeyDriver\Cleanup-TheSpikeyDriverEnvironment.ps1
.\TheSpikeyDriver\Cleanup-TheSpikeyDriverEnvironment.ps1
.\TheSpikeyDriver\Redeploy-TheSpikeyDriver.ps1 -Configuration Debug -SkipBuild
```

After a redeploy on this rig, reboot or relog before judging real keyboard input behavior. `--stats` may work before the keyboard input path is fully alive.

## Notes

- Do not move or delete the historical Downloads copy until this relocated source has been fully verified.
- Do not commit build outputs, WPP traces, ETL files, or captured evidence.
- The old private WDF control interface is not the product control path. The live control path is the HID vendor feature-report collection.
