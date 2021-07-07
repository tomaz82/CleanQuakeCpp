workspace "CleanQuakeCpp"
	platforms {
		"x32",
		"x64",
	}
	configurations {
		"Debug",
		"Release",
	}

project "CleanQuakeCpp"
	kind "WindowedApp"
	language "C++"
	cppdialect "C++98"
	characterset "MBCS"
	location "build/%{prj.name}"

	debugdir "E:/-=- Games -=-/CleanQuake/"

	debugargs {
	}

	editandcontinue "Off"
	warnings "Extra"

	flags {
		"MultiProcessorCompile",
		"NoMinimalRebuild",
	}

	files {
		"source/**.cpp",
		"source/**.h",
	}

	excludes {
	}

	defines {
		"_WINDOWS",
		"_ALLOW_RTCc_IN_STL",
		"_CRT_SECURE_NO_WARNINGS",
		"WIN32_LEAN_AND_MEAN",
	}

	includedirs {
	}

	libdirs {
	}

	links {
		"dsound",
		"opengl32",
		"ws2_32",
	}

-- filter

	filter { "configurations:Debug" }
		optimize "Off"
		symbols "On"

		defines {
			"_DEBUG",
			"DEBUG",
		}

	filter { "configurations:Release" }
		optimize "Speed"
		symbols "Off"

		flags {
			"LinkTimeOptimization",
		}

		defines {
			"NDEBUG",
			"RELEASE",
		}

	filter { "platforms:Win32" }
		defines {
			"WIN32",
		}

	filter { "platforms:x64" }
		defines {
			"WIN64",
		}
