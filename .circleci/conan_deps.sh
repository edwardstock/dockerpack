#!/usr/bin/env bash

conan install toolbox/3.1.1@edwardstock/latest --build=missing
conan install boost/1.72.0@ --build=missing
conan install nlohmann_json/3.7.3@ --build=missing
conan install yaml-cpp/0.6.3@ --build=missing
conan install libsodium/1.0.18@ --build=missing
