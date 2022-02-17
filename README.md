# Scoop-Search
[![Build Release](https://github.com/chriscp-cat/Scoop-Search/actions/workflows/build.yaml/badge.svg)](https://github.com/chriscp-cat/Scoop-Search/actions/workflows/build.yaml)

Alternative to  ```scoop search``` 


## Build

* vcpkg install jsoncpp
* open with Visual Studio

## Hook

Add this to your Powershell profile ```$profile```
```powershell
Invoke-Expression (&Scoop-Search --hook)
```
