#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <cstdio>
#include <iostream>
using namespace std;

#include "GCoptimization.h"
#include "niiIO.h"

// tuning parameter for data and smoothness energies.
float DATA_LAMBDA = 1.0;
float SMOOTH_FA_LAMBDA = 1.0;
float SMOOTH_PRINCIPAL_LAMBDA = 1.0;
float SMOOTH_OMNIBUS_LAMBDA = 1.0;



struct Vector {
  float x;
  float y;
  float z;
};

struct Image {
  float *dat;
  int w;
  int h;
  int d;
  const char *file;
};

struct Eigenvector {
  Image val;
  Image x;
  Image y;
  Image z;
};

struct Tensor {
  Eigenvector primary;
  Eigenvector secondary;
  Eigenvector tertiary;
  Image fa;
};


float dot(Vector a, Vector b) {
  return a.x*b.x + a.y*b.y + a.z*b.z;
};

// returns the index in the image array for the voxel at coordinates (i,j,k) 
// given the dimensions of the 3d image.
int indexOf(int i, int j, int k, Image image) {
  return ( k*image.w*image.h ) + ( j*image.w ) + i;
};

Image getImage(const char *filename) {
  Image got;
  niiIO *nio = new niiIO(filename);
  got.dat = nio->data;
  got.w = nio->width;
  got.h = nio->height;
  got.d = nio->depth;
  got.file = filename;
  return got;
};

Eigenvector getEigenvector(const char *val, const char *x, const char*y, const char *z) {
  Eigenvector got;
  got.val = getImage(val);
  got.x = getImage(x);
  got.y = getImage(y);
  got.z = getImage(z);
  return got;
};

Eigenvector newEigenvector(int width, int height, int depth) {
  Eigenvector E;
  E.val.dat = new float[width*height*depth]; E.val.file = "eigenvalue";
  E.x.dat = new float[width*height*depth]; E.x.file = "eigenvector_x";
  E.y.dat = new float[width*height*depth]; E.y.file = "eigenvector_y";
  E.z.dat = new float[width*height*depth]; E.z.file = "eigenvector_z";
  E.val.w = width; E.x.w = width; E.y.w = width; E.z.w = width;
  E.val.h = height; E.x.h = height; E.y.h = height; E.z.h = height;
  E.val.d = depth; E.x.d = depth; E.y.d = depth; E.z.d = depth;
  return E;
}

// returns a new eigenvector struct which contains the larger eigenvalue and eigenvector
// of two given objects at every voxel.
Eigenvector largerOf(Eigenvector a, Eigenvector b) {
  int width = a.val.w; int height = a.val.h; int depth = a.val.d;
  Eigenvector Large = newEigenvector(width,height,depth);
  for (int k = 0; k < depth; k++) {
    for (int j = 0; j < height; j++) {
      for (int i = 0; i < width; i++) {
        int idx = indexOf(i,j,k,Large.val);
        if (a.val.dat[idx] >= b.val.dat[idx]) {
          Large.val.dat[idx] = a.val.dat[idx];
          Large.x.dat[idx] = a.x.dat[idx];
          Large.y.dat[idx] = a.y.dat[idx];
          Large.z.dat[idx] = a.z.dat[idx];
        } else {
          Large.val.dat[idx] = b.val.dat[idx];
          Large.x.dat[idx] = b.x.dat[idx];
          Large.y.dat[idx] = b.y.dat[idx];
          Large.z.dat[idx] = b.z.dat[idx];
        }
      }
    }
  }
  return Large;
}

Eigenvector extractPrincipalEigenvector(Eigenvector a, Eigenvector b, Eigenvector c) {
  Eigenvector E;
  E = largerOf(a,b);
  E = largerOf(E,c);
  return E;
}


// Data penalty 
float dataCostFor(float fractional_anisotropy, int label) {
  return DATA_LAMBDA * (fractional_anisotropy - label) * (fractional_anisotropy - label);
};

// Smoothness cost. Inputs are the principal eigenvectors of neigboring voxels adjusted
// by their eigenvalues.  i.e.  V_adj = eigenval * eigenvec  (for largest eigenval in tensor).
float smoothCostFor(Vector Va, float FAa, Vector Vb, float FAb) {
  float roundness = SMOOTH_FA_LAMBDA * ( 1 / ( FAa + FAb ));
  float tubeyness = SMOOTH_PRINCIPAL_LAMBDA * FAa * FAb * dot(Va,Vb);
  float omnibus_penalty = SMOOTH_OMNIBUS_LAMBDA * (FAa - FAb) * (FAa - FAb);
  return (roundness-tubeyness)*(roundness-tubeyness) - omnibus_penalty;
};

// prints a summary of an Image structure
void summarize(Image im) {
  float min=1000000, max=-1000000;
  double sum=0;
  int vox_gt_zero = 0;
  
  for (int k = 0; k < im.d; k++) {
    for (int j = 0; j < im.h; j++) {
      for (int i = 0; i < im.w; i++) {
        if ( im.dat[indexOf(i,j,k,im)] < min ) min = im.dat[indexOf(i,j,k,im)];
        if ( im.dat[indexOf(i,j,k,im)] > max ) max = im.dat[indexOf(i,j,k,im)];
        if ( im.dat[indexOf(i,j,k,im)] > 0.0 ) {
          sum += im.dat[indexOf(i,j,k,im)];
          vox_gt_zero+=1;
        }
      }
    }
  }
  float avg = sum/(double)(vox_gt_zero);
  printf("Properties for %s\n", im.file);
  printf("%20s :\t%d %d %d\n", "Dimensions", im.w, im.h, im.d);
  printf("%20s :\t%0.15f\n", "Minimum", min);
  printf("%20s :\t%0.15f\n", "Maximum", max);
  printf("%20s :\t%0.15f\n", "Non-zero Mean", avg);
}

void summarize(Eigenvector e) {
  summarize(e.val);
  summarize(e.x);
  summarize(e.y);
  summarize(e.z);
}

Vector vectorAt(int idx, Eigenvector v) {
  Vector got;
  got.x = v.val.dat[idx] * v.x.dat[idx];
  got.y = v.val.dat[idx] * v.y.dat[idx];
  got.z = v.val.dat[idx] * v.z.dat[idx];
  return got;
}




//
int main (int argc, char const *argv[]) {
  int NUM_LABELS = 2;
  
  Eigenvector e1 = getEigenvector(argv[1],argv[2],argv[3],argv[4]);
  Eigenvector e2 = getEigenvector(argv[5],argv[6],argv[7],argv[8]);
  Eigenvector e3 = getEigenvector(argv[9],argv[10],argv[11],argv[12]);
  Image fa = getImage(argv[13]);
  
  Eigenvector E = extractPrincipalEigenvector(e1,e2,e3);
  
//  summarize(e1);
//  summarize(e2);
//  summarize(e3);
//  summarize(E);
  
  int width = e1.val.w;
  int height = e1.val.h;
  int depth = e1.val.d;
  
  int nvox = width*height*depth;
  
  float *result = new float[nvox];   // stores result of optimization
  
  try{
    GCoptimizationGeneralGraph *gc = new GCoptimizationGeneralGraph(nvox, NUM_LABELS);
    int *datacosts = new int[nvox*NUM_LABELS];
    int *smoothcosts = new int[NUM_LABELS*NUM_LABELS];
    
    // the smooth cost matrix is 1 for differing labels, 0 for same labels. This means that
    // the smoothness costs set up in the neighbor system only count for neighbors with
    // opposite labels.
    for (int l1 = 0; l1 < NUM_LABELS; l1++) {
      for (int l2 = 0; l2 < NUM_LABELS; l2++) {
        smoothcosts[l1 + NUM_LABELS*l2] = l1 != l2 ? 1 : 0;
      }
    }
    
    // look at every voxel
    for (int i = 0; i < width; i++) {
      for (int j = 0; j < height; j++) {
        for (int k = 0; k < depth; k++) {
          int here = indexOf(i,j,k,e1.val);
          int left = indexOf(i-1,j,k,e1.val);
          int down = indexOf(i,j-1,k,e1.val);
          int bottom = indexOf(i,j,k-1,e1.val);
          
          // calculate the data costs for this voxel
          for ( int label = 0; label < NUM_LABELS; label++) {
            datacosts[here*NUM_LABELS + label] = dataCostFor(fa.dat[here], label);
          }
          
          Vector vhere = vectorAt(here, E);
          
          // add the edges to left, down, and bottom if they are in the image.
          if ( 0 <= left && left < nvox ) {
            Vector vleft = vectorAt(left, E);
            gc->setNeighbors(here, left, smoothCostFor(vleft, fa.dat[left], vhere, fa.dat[here]));
          }
          if ( 0 <= down && down < nvox ) {
            Vector vdown = vectorAt(down, E);
            gc->setNeighbors(here, down, smoothCostFor(vdown, fa.dat[down], vhere, fa.dat[here]));
          }
          if ( 0 <= bottom && bottom < nvox ) {
            Vector vbottom = vectorAt(bottom, E);
            gc->setNeighbors(here, bottom, smoothCostFor(vbottom, fa.dat[bottom], vhere, fa.dat[here]));
          }
        }
      }
    }
    
    gc->setDataCost(datacosts);
    gc->setSmoothCost(smoothcosts);
    
    printf("Before optimization energy is :\t%d\n",gc->compute_energy());
    gc->expansion(2); // run expansion for 2 iterations. For swap use gc->swap(num_iterations);
    printf("After optimization energy is :\t%d\n",gc->compute_energy());
    
    for ( int  i = 0; i < nvox; i++ ) {
      result[i] = gc->whatLabel(i);
    }
    
    delete gc;
    delete [] smoothcosts;
    delete [] datacosts;
  }
  catch (GCException e) {
    e.Report();
  }
  
  //  write result to file
  niiIO *nio = new niiIO(argv[1]);
  nio->writeData(result, nvox, "test.nii");
  
  delete [] result;
  
  return 0;
}


