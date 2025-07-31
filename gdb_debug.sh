#!/usr/bin/bash

gdb -args ../pipedal/build/src/pipedald  /etc/pipedal/config ../pipedal/vite/dist -port 0.0.0.0:8080 -log-level debug
