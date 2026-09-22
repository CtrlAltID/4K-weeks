# Third-party notices

## Line Dash

The dial, pill mechanism (shake-to-cycle through pages, battery fill), and
bitmap numeral rendering in `src/c/4K-weeks.c` are adapted from **Line Dash**
(<https://github.com/pagnotta/pebble-line-dash>) by Patrick Heeren
(pagnotta), used under the MIT license below. The pill's reveal animation was
reworked from the original's slide-in-from-the-top into a grow/shrink from
its own center. The weeks-left pill, its settings (birthday, gender,
country, location estimate), and `src/c/life_expectancy.h` are new
additions, not part of the original.

```
MIT License

Copyright (c) 2026 Patrick Heeren

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

Fonts (`resources/fonts/`) are Jost and Montserrat, both under the SIL Open
Font License — see the accompanying `*-OFL.txt` files.
