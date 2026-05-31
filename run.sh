#!/bin/bash
bash build.sh && LSAN_OPTIONS=suppressions=build.lsan.supp ./pong
