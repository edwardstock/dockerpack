# Release notes

## 0.2.0
* Added `stateless` option to command step. It can be used if some step shouldn't be added to success state. In next re-run this step will be executed again.
* Added ability to add pre-steps to multijob special config
* Added ability to set global environments in root of config. They will be added to each execution unit
* Added ability to exclude job by name (not working yet for image building) or image by adding **!** before pattern. Example: `dockerpack build -n !debian` will execute all jobs except all debian (job name or image)

## 0.1.2
* Fixed creating working directory
* Fixed --copy-local

## 0.1.1
* Automatically create workdir if not exists
* Added `--copy-local` argument. This function skips `checkout` command and copy all from current directory to working directory
* Added `-e | --env` argument to add special environment variables while building. Value from this argument will overwrite others environments
* Automatically normalize local and remote paths, and now you can for example copy files to `~` or using environments `copy: ~/my_project ~/builds/my_project`
* Also now able to set `copy` command without special `$image` variable. Just set `copy: ~/my_project ~/my_project` or just `copy: ~/my_project`. In the second case, local `~/my_project` will be copied to container `~/my_project`
