param (
	[string]$toolset = "vs2017"
)

# Let the environment override a toolset, if there is an environment variable set.
if ($env:toolset)
{
	$toolset = $env:toolset
}

$parameters = ""

# Append the toolset.
$parameters = "$parameters $toolset"
$executable = "utils\premake5"

$process = Start-Process $executable $parameters -Wait -NoNewWindow -PassThru
if ($process.ExitCode -ne 0) { throw "Project generation error " + $process.ExitCode }
