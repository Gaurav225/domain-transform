#ifndef NC_H
#define NC_H

#include "common.h"
#include <cstring>

namespace NC
{

void TransformedDomainBoxFilter(Mat2<float3>& img,
                                Mat2<float> dIdx, float boxR)
{
    FP_CALL_START(FunP::ID_boxFilter);

    const uint W = img.width;
    const uint H = img.height;

    std::vector<float3> imgRow(W);
    std::vector<float3> imgRow1(W);

    for (uint i=0; i<H; i++)
    {
        uint i1 = i+1;

        uint posL = 0;
        uint posR = 0;

        uint posL1 = 0;
        uint posR1 = 0;

        memcpy(&imgRow[0],&img.data[i*W],sizeof(float3)*W);
        memcpy(&imgRow1[0],&img.data[i1*W],sizeof(float3)*W);

        // row sat
        float3 sum; sum.r = sum.g = sum.b = 0;
        float3 sum1; sum1.r = sum1.g = sum1.b = 0;

        for (uint j=0; j<W; j++)
        {
            uint idx = i*W + j;
            uint idx1 = i1*W +j;

            // compute box filter bounds
            float dtL = dIdx.data[idx] - boxR;
            float dtR = dIdx.data[idx] + boxR;

            float dtL1 = dIdx.data[idx1] - boxR;
            float dtR1 = dIdx.data[idx1] + boxR;

            // update box filter window
            while (dIdx.data[i*W+posL] < dtL && posL < W-1)
            {
                sum.r -= imgRow[posL].r;
                sum.g -= imgRow[posL].g;
                sum.b -= imgRow[posL].b;
                posL++;
            }
            while (posR < W && dIdx.data[i*W+posR] < dtR )  // attention, allows for index = W
            {
                sum.r += imgRow[posR].r;
                sum.g += imgRow[posR].g;
                sum.b += imgRow[posR].b;
                posR++;
            }

            while (dIdx.data[i1*W+posL1] < dtL1 && posL1 < W-1)
            {
                sum1.r -= imgRow1[posL1].r;
                sum1.g -= imgRow1[posL1].g;
                sum1.b -= imgRow1[posL1].b;
                posL1++;
            }
            while (posR1 < W && dIdx.data[i1*W+posR1] < dtR1 )  // attention, allows for index = W
            {
                sum1.r += imgRow1[posR1].r;
                sum1.g += imgRow1[posR1].g;
                sum1.b += imgRow1[posR1].b;
                posR1++;
            }

            int delta = posR - posL;
            float invD = 1.0f / delta;

            int delta1 = posR1 - posL1;
            float invD1 = 1.0f / delta1;

            img.data[idx].r = sum.r * invD;
            img.data[idx].g = sum.g * invD;
            img.data[idx].b = sum.b * invD;

            img.data[idx].r = sum.r * invD1;
            img.data[idx].g = sum.g * invD1;
            img.data[idx].b = sum.b * invD1;
        }
    }

    FP_CALL_END(FunP::ID_boxFilter);
}


void filter(Mat2<float3>& img, float sigma_s, float sigma_r, uint nIterations)
{
    FP_CALL_START(FunP::ID_ALL);
    // Estimate horizontal and vertical partial derivatives using finite differences.

    // Compute the l1-norm distance of neighbor pixels.
    FP_CALL_START(FunP::ID_domainTransform);

    const uint W = img.width;
    const uint H = img.height;

    float s = sigma_s/sigma_r;

    Mat2<float> dIdx(W,H);
    Mat2<float> dIdy(W,H);

    for (uint j=0; j<W; j++)
    {
        dIdy.data[j] = 1.0f;
    }

    for (uint i=0; i<H-1; i++)
    {
        dIdx.data[i*W] = 1.0f;
        for (uint j=0; j<W-1; j++)
        {
            uint idx = i*W + j;
            dIdx.data[idx+1] = 1.0f +
                               s*(fabs(img.data[idx+1].r - img.data[idx].r) +
                                  fabs(img.data[idx+1].g - img.data[idx].g) +
                                  fabs(img.data[idx+1].b - img.data[idx].b));
            dIdy.data[idx+W] = 1.0f +
                               s*(fabs(img.data[idx+W].r - img.data[idx].r) +
                                  fabs(img.data[idx+W].g - img.data[idx].g) +
                                  fabs(img.data[idx+W].b - img.data[idx].b));
        }
    }

    dIdx.data[(H-1)*W] = 1.0f;
    for (uint j=0; j<W-1; j++)
    {
        uint idx = (H-1)*W + j;
        dIdx.data[idx+1] = 1.0f +
                           s*(fabs(img.data[idx+1].r - img.data[idx].r) +
                              fabs(img.data[idx+1].g - img.data[idx].g) +
                              fabs(img.data[idx+1].b - img.data[idx].b));
    }

    for (uint i=0; i<H-1; i++)
    {
        uint idx = i*W + (W-1);
        dIdy.data[idx+W] = 1.0f +
                           s*(fabs(img.data[idx+W].r - img.data[idx].r) +
                              fabs(img.data[idx+W].g - img.data[idx].g) +
                              fabs(img.data[idx+W].b - img.data[idx].b));
    }


    FP_CALL_END(FunP::ID_domainTransform);

    Mat2<float> dIdyT(H,W);

    cumsumX(dIdx);
    transposeB(dIdy,dIdyT);
    cumsumX(dIdyT);

    Mat2<float3> imgT(img.height,img.width);

    for(uint i=0; i<nIterations; i++)
    {
        // Compute the sigma value for this iteration
        float sigmaHi = sigma_s * sqrt(3) * powf(2.0,(nIterations - (i+1))) / sqrt(powf(4,nIterations)-1);

        // Compute the radius of the box filter with the desired variance.
        float boxR = sqrt(3) * sigmaHi;

        TransformedDomainBoxFilter(img, dIdx, boxR);
        transposeB(img,imgT);

        TransformedDomainBoxFilter(imgT,dIdyT,boxR);
        transposeB(imgT,img);
    }


    // cleanup
    dIdx.free();
    dIdy.free();
    dIdyT.free();

    imgT.free();

    FP_CALL_END(FunP::ID_ALL);
}



}

#endif // NC_H
