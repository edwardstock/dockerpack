DockerPack
----------------
DockerPack is a small simple local stateful docker-based CI to test and deploy projects that should be built on multiple environments.

[![CircleCI](https://circleci.com/gh/edwardstock/dockerpack/tree/master.svg?style=svg)](https://circleci.com/gh/edwardstock/dockerpack/tree/master)


## Features
Build your own images to build and test your projects faster than cloud CI, as
dockerpack supports build state and you can any time access interactive shell to fix some issues for specified image.<br/>
DockerPack saves success build steps to avoid boilerplate rework.<br/>
Also **dockerpack.yaml** syntax almost looks like github actions or CircleCI.


## Build requirements
* cmake >= 3.12
* gnu gcc 5+/clang 5+/msvc
* make
* toolbox/3.1.1
* boost 1.72.0
* nlohmann/json
* yaml-cpp
* libsodium


## Download

### RPM
Supported distributions:
- centos 7, 8
- fedora 32-34

Steps:
* Create `bintray-edwardstock.repo` file inside `/etc/yum.repos.d`
* Add below to this file
```ini
[bintray-edwardstock]
name=bintray-edwardstock
baseurl=https://dl.bintray.com/edwardstock/rh/[centos OR fedora]/$releasever/$basearch
gpgcheck=0
repo_gpgcheck=1
enabled=1
```
* Add repository gpg to trusted
```bash
curl -s https://bintray.com/user/downloadSubjectPublicKey?username=bintray | gpg --import
```
* Update repository `yum -y update` or `dnf update`
* Install  `yum install dockerpack`

### DEB
Supported distributions:
- debian: stretch, buster
- ubuntu: xenial, bionic, focal, groovy

```bash
echo "deb https://dl.bintray.com/edwardstock/debian {distribution} main" | sudo tee -a /etc/apt/sources.list
curl -s https://bintray.com/user/downloadSubjectPublicKey?username=bintray | sudo apt-key add -
apt update && apt install dockerpack
```