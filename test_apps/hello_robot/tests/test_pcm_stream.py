#!/usr/bin/env python3
"""Host verification of the proposed mono 24k->16k sample mapping.
Does not validate microphone slot selection or feature extraction."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
C = r'''
#include <stdint.h>
#include <stdio.h>
#include "pcm_stream.h"
int main(void) {
 int16_t src[HELLO_AUDIO_SOURCE_FRAMES * 2], dst[HELLO_AUDIO_TARGET_FRAMES];
 for (int i = 0; i < HELLO_AUDIO_SOURCE_FRAMES; ++i) {
   src[2*i] = (int16_t)(i * 10);
   src[2*i+1] = -1234;
 }
 hello_pcm_24k_to_16k_left(src, dst);
 for (int i = 0; i < HELLO_AUDIO_TARGET_FRAMES; ++i) {
   int expected = (i % 2 == 0) ? (3*(i/2)*10) : ((3*(i/2)+1)*10+(3*(i/2)+2)*10)/2;
   if (dst[i] != expected) { printf("failed at %d: %d vs %d\n",i,dst[i],expected); return 1; }
 }
 printf("PASS: 240 stereo frames -> 160 mono frames; left-channel mapping\n");
 return 0;
}
'''
def main():
    with tempfile.TemporaryDirectory() as d:
        source = Path(d)/"check.c"
        binary = Path(d)/"check"
        source.write_text(C)
        subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                        "-I", str(ROOT/"main"), str(source), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
if __name__ == "__main__":
    main()
