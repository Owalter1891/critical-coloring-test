/*
* A new heuristic for finding verifiable k-vertex-critical subgraphs
* 
* Copyright (c) 2022 Alex Gliesch, Marcus Ritt
* 
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
* 
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPY lRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/
#pragma once       
#include "main.h"
struct subgraph {
  subgraph() = default;
  subgraph(vi ss) { set_ss(move(ss)); }
  subgraph(int sz) { set_size(sz); }
  void set_size(int sz) {
    ss.assign(sz, -1);
    n = sz;
  }
  void set_ss(vi ss2);
  subgraph& update_all();
  int m_cost_swap(int i, int v) {
    return m - deg[ss[i]] + deg[v] - AM[ss[i]][v];
  }
  void swap(int i, int v);
  vi deg;
  vi ss;
  int n = 0;
  int m = 0;
};
