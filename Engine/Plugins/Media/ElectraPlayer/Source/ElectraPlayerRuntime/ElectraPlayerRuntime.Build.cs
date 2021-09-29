// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;
using System.IO;

namespace UnrealBuildTool.Rules
{
	public class ElectraPlayerRuntime: ModuleRules
	{
		public ElectraPlayerRuntime(ReadOnlyTargetRules Target) : base(Target)
		{
			//
			// Common setup...
			//

            bLegalToDistributeObjectCode = true;
            PCHUsage = PCHUsageMode.NoPCHs;

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"HTTP",
					"WebSockets",
					"Json",
					"ElectraBase",
					"ElectraCDM",
					"XmlParser"
				});
			if (Target.bCompileAgainstEngine)
			{
				// Added to allow debug rendering if used in UE context
				PrivateDependencyModuleNames.Add("Engine");
			}

			PrivateIncludePaths.AddRange(
				new string[] {
					"ElectraPlayerRuntime/Private/Runtime",
				});

			//
			// Common platform setup...
			//

			if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
            {
                string DirectXSDKDir = Target.UEThirdPartySourceDirectory + "Windows/DirectX";
                PublicSystemIncludePaths.Add(DirectXSDKDir + "/include");

				if (Target.Platform == UnrealTargetPlatform.Win64)
                {
                    DirectXSDKDir += "/Lib/x64/";
                }
                else if (Target.Platform == UnrealTargetPlatform.Win32)
                {
                    DirectXSDKDir += "/Lib/x86/";
                }

				PrivateDefinitions.Add("_CRT_SECURE_NO_WARNINGS=1");

                PrivateDefinitions.Add("ELECTRA_ENABLE_MFDECODER");
                PrivateDefinitions.Add("ELECTRA_ENABLE_SWDECODE");

                AddEngineThirdPartyPrivateStaticDependencies(Target, "WinHttp");

				if (WinSupportsDX9())
				{
					AddEngineThirdPartyPrivateStaticDependencies(Target, "DX9");
					PrivateDefinitions.Add("ELECTRA_HAVE_DX9");     // video decoding on Win7 enabled - uses DX9
				}

				if (WinSupportsDX11())
				{
					PrivateDefinitions.Add("ELECTRA_HAVE_DX11");    // video decoding for DX11 enabled (Win8+)
				}

				if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32)
				{
					PublicAdditionalLibraries.AddRange(new string[] {
						DirectXSDKDir + "dxerr.lib",
					});
				}

				PublicSystemLibraries.AddRange(new string[] {
                    "strmiids.lib",
                    "legacy_stdio_definitions.lib",
                    "Dxva2.lib",
                });

                // Delay-load all MF DLLs to be able to check Windows version for compatibility in `StartupModule` before loading them manually
                PublicSystemLibraries.Add("mfplat.lib");
                PublicDelayLoadDLLs.Add("mfplat.dll");
                PublicSystemLibraries.Add("mfuuid.lib");

				PublicIncludePaths.Add("$(ModuleDir)/Public/Windows");
				PrivateIncludePaths.Add("ElectraPlayerRuntime/Private/Runtime/Decoder/Windows");
				PrivateIncludePaths.Add("ElectraPlayerRuntime/Private/Windows");
			}
			else if (Target.Platform == UnrealTargetPlatform.Mac)
            {
                PublicFrameworks.AddRange(
				new string[] {
								"CoreMedia",
								"CoreVideo",
								"AVFoundation",
								"AudioToolbox",
                                "VideoToolbox",
								"QuartzCore"
				});

				PublicIncludePaths.Add("$(ModuleDir)/Public/Apple");

				PrivateIncludePaths.Add("ElectraPlayerRuntime/Private/Runtime/Decoder/Apple");
			}
			else if (Target.Platform == UnrealTargetPlatform.IOS )
            {
				PublicFrameworks.AddRange(
				new string[] {
								"CoreMedia",
								"CoreVideo",
								"AVFoundation",
								"AudioToolbox",
                                "VideoToolbox",
								"QuartzCore"
				});

				PublicIncludePaths.Add("$(ModuleDir)/Public/Apple");

				PrivateIncludePaths.Add("ElectraPlayerRuntime/Private/Runtime/Decoder/Apple");
			}
			else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Android) )
            {
				PublicDefinitions.Add("CURL_ENABLE_DEBUG_CALLBACK=1");

                if (Target.Configuration != UnrealTargetConfiguration.Shipping)
                {
                    PublicDefinitions.Add("CURL_ENABLE_NO_TIMEOUTS_OPTION=1");
                }

                string PluginPath = Utils.MakePathRelativeTo(ModuleDirectory, Target.RelativeEnginePath);
                AdditionalPropertiesForReceipt.Add("AndroidPlugin", Path.Combine(PluginPath, "ElectraPlayerRuntime_UPL.xml"));

				PublicIncludePaths.Add("$(ModuleDir)/Public/Android");

				PrivateIncludePaths.Add("ElectraPlayerRuntime/Private/Runtime/Decoder/Android");
			}
			else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix) )
            {
				PublicDefinitions.Add("CURL_ENABLE_DEBUG_CALLBACK=1");

                if (Target.Configuration != UnrealTargetConfiguration.Shipping)
                {
                    PublicDefinitions.Add("CURL_ENABLE_NO_TIMEOUTS_OPTION=1");
                }

				PublicIncludePaths.Add("$(ModuleDir)/Public/Unix");

				PrivateIncludePaths.Add("ElectraPlayerRuntime/Private/Runtime/Decoder/GStreamer");
			}
		}

		protected virtual bool WinSupportsDX9()
		{
			return true;
		}

		protected virtual bool WinSupportsDX11()
		{
			return true;
		}
	}
}
