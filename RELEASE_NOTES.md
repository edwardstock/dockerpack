# Release notes

## 0.1.1
* Automatically create workdir if not exists
* Added `--copy-local` argument. This function skips `checkout` command and copy all from current directory to working directory
* Added `-e | --env` argument to add special environment variables while building. Value from this argument will overwrite others environments
* Automatically normalize local and remote paths, and now you can for example copy files to `~` or using environments `copy: ~/my_project ~/builds/my_project`
* Also now able to set `copy` command without special `$image` variable. Just set `copy: ~/my_project ~/my_project` or just `copy: ~/my_project`. In the second case, local `~/my_project` will be copied to container `~/my_project`
