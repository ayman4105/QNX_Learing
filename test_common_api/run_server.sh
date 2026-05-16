#!/bin/bash
export VSOMEIP_CONFIGURATION=$PWD/Config/vsomeip-server.json
export VSOMEIP_APPLICATION_NAME=HelloWorldService
$PWD/build-server-linux/HelloWorldServer
