-- premake5.lua
workspace "Radar"
   architecture "x64"
   configurations { "Debug", "Release", "Dist" }
   startproject "Radar"

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"
include "Walnut/WalnutExternal.lua"

include "Radar"