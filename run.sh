#!/bin/bash
bash build.sh && LSAN_OPTIONS=suppressions=run.lsan.supp ./pong
