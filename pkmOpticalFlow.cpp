/*
 
 © Parag K Mital, parag@pkmital.com
 
 The Software is and remains the property of Parag K Mital
 ("pkmital") The Licensee will ensure that the Copyright Notice set
 out above appears prominently wherever the Software is used.
 
 The Software is distributed under this Licence:
 
 - on a non-exclusive basis,
 
 - solely for non-commercial use in the hope that it will be useful,
 
 - "AS-IS" and in order for the benefit of its educational and research
 purposes, pkmital makes clear that no condition is made or to be
 implied, nor is any representation or warranty given or to be
 implied, as to (i) the quality, accuracy or reliability of the
 Software; (ii) the suitability of the Software for any particular
 use or for use under any specific conditions; and (iii) whether use
 of the Software will infringe third-party rights.
 
 pkmital disclaims:
 
 - all responsibility for the use which is made of the Software; and
 
 - any liability for the outcomes arising from using the Software.
 
 The Licensee may make public, results or data obtained from, dependent
 on or arising out of the use of the Software provided that any such
 publication includes a prominent statement identifying the Software as
 the source of the results or the data, including the Copyright Notice
 and stating that the Software has been made available for use by the
 Licensee under licence from pkmital and the Licensee provides a copy of
 any such publication to pkmital.
 
 The Licensee agrees to indemnify pkmital and hold them
 harmless from and against any and all claims, damages and liabilities
 asserted by third parties (including claims for negligence) which
 arise directly or indirectly from the use of the Software or any
 derivative of it or the sale of any products based on the
 Software. The Licensee undertakes to make no liability claim against
 any employee, student, agent or appointee of pkmital, in connection
 with this Licence or the Software.
 
 
 No part of the Software may be reproduced, modified, transmitted or
 transferred in any form or by any means, electronic or mechanical,
 without the express permission of pkmital. pkmital's permission is not
 required if the said reproduction, modification, transmission or
 transference is done without financial return, the conditions of this
 Licence are imposed upon the receiver of the product, and all original
 and amended source code is included in any transmitted product. You
 may be held legally responsible for any copyright infringement that is
 caused or encouraged by your failure to abide by these terms and
 conditions.
 
 You are not permitted under this Licence to use this Software
 commercially. Use for which any financial return is received shall be
 defined as commercial use, and includes (1) integration of all or part
 of the source code or the Software into a product for sale or license
 by or on behalf of Licensee to third parties or (2) use of the
 Software or any derivative of it for research with the final aim of
 developing software products for sale or license to a third party or
 (3) use of the Software or any derivative of it for research with the
 final aim of developing non-software products for sale or license to a
 third party, or (4) use of the Software to provide any service to an
 external organisation for which payment is received. If you are
 interested in using the Software commercially, please contact pkmital to
 negotiate a licence. Contact details are: parag@pkmital.com
 
 */


//
//  pkmOpticalFlow.cpp
//  ofxOpenCV2461Example
//
//  Created by Mr. Magoo on 9/16/13.
//
//

#include "pkmOpticalFlow.h"

namespace cv {

    tvl1flow::tvl1flow()
    {
        tau            = 0.25;
        lambda         = 0.15;
        theta          = 0.3;
        nscales        = 3;
        warps          = 1;
        epsilon        = 0.05;
        iterations     = 50;
        useInitialFlow = false;
    }

    void tvl1flow::calc(InputArray _I0, InputArray _I1, InputOutputArray _flow)
    {
        Mat I0 = _I0.getMat();
        Mat I1 = _I1.getMat();
        
        CV_Assert( I0.type() == CV_8UC1 || I0.type() == CV_32FC1 );
        CV_Assert( I0.size() == I1.size() );
        CV_Assert( I0.type() == I1.type() );
        CV_Assert( !useInitialFlow || (_flow.size() == I0.size() && _flow.type() == CV_32FC2) );
        CV_Assert( nscales > 0 );
        
        // allocate memory for the pyramid structure
        I0s.resize(nscales);
        I1s.resize(nscales);
        u1s.resize(nscales);
        u2s.resize(nscales);
        
        I0.convertTo(I0s[0], I0s[0].depth(), I0.depth() == CV_8U ? 1.0 : 255.0);
        I1.convertTo(I1s[0], I1s[0].depth(), I1.depth() == CV_8U ? 1.0 : 255.0);
        
        u1s[0].create(I0.size());
        u2s[0].create(I0.size());
        
        if (useInitialFlow)
        {
            Mat_<float> mv[] = {u1s[0], u2s[0]};
            split(_flow.getMat(), mv);
        }
        
        I1x_buf.create(I0.size());
        I1y_buf.create(I0.size());
        
        flowMap1_buf.create(I0.size());
        flowMap2_buf.create(I0.size());
        
        I1w_buf.create(I0.size());
        I1wx_buf.create(I0.size());
        I1wy_buf.create(I0.size());
        
        grad_buf.create(I0.size());
        rho_c_buf.create(I0.size());
        
        v1_buf.create(I0.size());
        v2_buf.create(I0.size());
        
        p11_buf.create(I0.size());
        p12_buf.create(I0.size());
        p21_buf.create(I0.size());
        p22_buf.create(I0.size());
        
        div_p1_buf.create(I0.size());
        div_p2_buf.create(I0.size());
        
        u1x_buf.create(I0.size());
        u1y_buf.create(I0.size());
        u2x_buf.create(I0.size());
        u2y_buf.create(I0.size());
        
        // create the scales
        for (int s = 1; s < nscales; ++s)
        {
            pyrDown(I0s[s - 1], I0s[s]);
            pyrDown(I1s[s - 1], I1s[s]);
            
            if (I0s[s].cols < 16 || I0s[s].rows < 16)
            {
                nscales = s;
                break;
            }
            
            if (useInitialFlow)
            {
                pyrDown(u1s[s - 1], u1s[s]);
                pyrDown(u2s[s - 1], u2s[s]);
                
                multiply(u1s[s], Scalar::all(0.5), u1s[s]);
                multiply(u2s[s], Scalar::all(0.5), u2s[s]);
            }
            else
            {
                u1s[s].create(I0s[s].size());
                u2s[s].create(I0s[s].size());
            }
        }
        
        if (!useInitialFlow)
        {
            u1s[nscales-1].setTo(Scalar::all(0));
            u2s[nscales-1].setTo(Scalar::all(0));
        }
        
        // pyramidal structure for computing the optical flow
        for (int s = nscales - 1; s >= 0; --s)
        {
            // compute the optical flow at the current scale
            procOneScale(I0s[s], I1s[s], u1s[s], u2s[s]);
            
            // if this was the last scale, finish now
            if (s == 0)
                break;
            
            // otherwise, upsample the optical flow
            
            // zoom the optical flow for the next finer scale
            resize(u1s[s], u1s[s - 1], I0s[s - 1].size());
            resize(u2s[s], u2s[s - 1], I0s[s - 1].size());
            
            // scale the optical flow with the appropriate zoom factor
            multiply(u1s[s - 1], Scalar::all(2), u1s[s - 1]);
            multiply(u2s[s - 1], Scalar::all(2), u2s[s - 1]);
        }
        
        Mat uxy[] = {u1s[0], u2s[0]};
        merge(uxy, 2, _flow);
    }

    ////////////////////////////////////////////////////////////
    // buildFlowMap

    struct BuildFlowMapBody : ParallelLoopBody
    {
        void operator() (const Range& range) const;
        
        Mat_<float> u1;
        Mat_<float> u2;
        mutable Mat_<float> map1;
        mutable Mat_<float> map2;
    };

    void BuildFlowMapBody::operator() (const Range& range) const
    {
        for (int y = range.start; y < range.end; ++y)
        {
            const float* u1Row = u1[y];
            const float* u2Row = u2[y];
            
            float* map1Row = map1[y];
            float* map2Row = map2[y];
            
            for (int x = 0; x < u1.cols; ++x)
            {
                map1Row[x] = x + u1Row[x];
                map2Row[x] = y + u2Row[x];
            }
        }
    }

    void buildFlowMap(const Mat_<float>& u1, const Mat_<float>& u2, Mat_<float>& map1, Mat_<float>& map2)
    {
        CV_DbgAssert( u2.size() == u1.size() );
        CV_DbgAssert( map1.size() == u1.size() );
        CV_DbgAssert( map2.size() == u1.size() );
        
        BuildFlowMapBody body;
        
        body.u1 = u1;
        body.u2 = u2;
        body.map1 = map1;
        body.map2 = map2;
        
        parallel_for_(Range(0, u1.rows), body);
    }

    ////////////////////////////////////////////////////////////
    // centeredGradient

    struct CenteredGradientBody : ParallelLoopBody
    {
        void operator() (const Range& range) const;
        
        Mat_<float> src;
        mutable Mat_<float> dx;
        mutable Mat_<float> dy;
    };

    void CenteredGradientBody::operator() (const Range& range) const
    {
        const int last_col = src.cols - 1;
        
        for (int y = range.start; y < range.end; ++y)
        {
            const float* srcPrevRow = src[y - 1];
            const float* srcCurRow = src[y];
            const float* srcNextRow = src[y + 1];
            
            float* dxRow = dx[y];
            float* dyRow = dy[y];
            
            for (int x = 1; x < last_col; ++x)
            {
                dxRow[x] = 0.5f * (srcCurRow[x + 1] - srcCurRow[x - 1]);
                dyRow[x] = 0.5f * (srcNextRow[x] - srcPrevRow[x]);
            }
        }
    }

    void centeredGradient(const Mat_<float>& src, Mat_<float>& dx, Mat_<float>& dy)
    {
        CV_DbgAssert( src.rows > 2 && src.cols > 2 );
        CV_DbgAssert( dx.size() == src.size() );
        CV_DbgAssert( dy.size() == src.size() );
        
        const int last_row = src.rows - 1;
        const int last_col = src.cols - 1;
        
        // compute the gradient on the center body of the image
        {
            CenteredGradientBody body;
            
            body.src = src;
            body.dx = dx;
            body.dy = dy;
            
            parallel_for_(Range(1, last_row), body);
        }
        
        // compute the gradient on the first and last rows
        for (int x = 1; x < last_col; ++x)
        {
            dx(0, x) = 0.5f * (src(0, x + 1) - src(0, x - 1));
            dy(0, x) = 0.5f * (src(1, x) - src(0, x));
            
            dx(last_row, x) = 0.5f * (src(last_row, x + 1) - src(last_row, x - 1));
            dy(last_row, x) = 0.5f * (src(last_row, x) - src(last_row - 1, x));
        }
        
        // compute the gradient on the first and last columns
        for (int y = 1; y < last_row; ++y)
        {
            dx(y, 0) = 0.5f * (src(y, 1) - src(y, 0));
            dy(y, 0) = 0.5f * (src(y + 1, 0) - src(y - 1, 0));
            
            dx(y, last_col) = 0.5f * (src(y, last_col) - src(y, last_col - 1));
            dy(y, last_col) = 0.5f * (src(y + 1, last_col) - src(y - 1, last_col));
        }
        
        // compute the gradient at the four corners
        dx(0, 0) = 0.5f * (src(0, 1) - src(0, 0));
        dy(0, 0) = 0.5f * (src(1, 0) - src(0, 0));
        
        dx(0, last_col) = 0.5f * (src(0, last_col) - src(0, last_col - 1));
        dy(0, last_col) = 0.5f * (src(1, last_col) - src(0, last_col));
        
        dx(last_row, 0) = 0.5f * (src(last_row, 1) - src(last_row, 0));
        dy(last_row, 0) = 0.5f * (src(last_row, 0) - src(last_row - 1, 0));
        
        dx(last_row, last_col) = 0.5f * (src(last_row, last_col) - src(last_row, last_col - 1));
        dy(last_row, last_col) = 0.5f * (src(last_row, last_col) - src(last_row - 1, last_col));
    }

    ////////////////////////////////////////////////////////////
    // forwardGradient

    struct ForwardGradientBody : ParallelLoopBody
    {
        void operator() (const Range& range) const;
        
        Mat_<float> src;
        mutable Mat_<float> dx;
        mutable Mat_<float> dy;
    };

    void ForwardGradientBody::operator() (const Range& range) const
    {
        const int last_col = src.cols - 1;
        
        for (int y = range.start; y < range.end; ++y)
        {
            const float* srcCurRow = src[y];
            const float* srcNextRow = src[y + 1];
            
            float* dxRow = dx[y];
            float* dyRow = dy[y];
            
            for (int x = 0; x < last_col; ++x)
            {
                dxRow[x] = srcCurRow[x + 1] - srcCurRow[x];
                dyRow[x] = srcNextRow[x] - srcCurRow[x];
            }
        }
    }

    void forwardGradient(const Mat_<float>& src, Mat_<float>& dx, Mat_<float>& dy)
    {
        CV_DbgAssert( src.rows > 2 && src.cols > 2 );
        CV_DbgAssert( dx.size() == src.size() );
        CV_DbgAssert( dy.size() == src.size() );
        
        const int last_row = src.rows - 1;
        const int last_col = src.cols - 1;
        
        // compute the gradient on the central body of the image
        {
            ForwardGradientBody body;
            
            body.src = src;
            body.dx = dx;
            body.dy = dy;
            
            parallel_for_(Range(0, last_row), body);
        }
        
        // compute the gradient on the last row
        for (int x = 0; x < last_col; ++x)
        {
            dx(last_row, x) = src(last_row, x + 1) - src(last_row, x);
            dy(last_row, x) = 0.0f;
        }
        
        // compute the gradient on the last column
        for (int y = 0; y < last_row; ++y)
        {
            dx(y, last_col) = 0.0f;
            dy(y, last_col) = src(y + 1, last_col) - src(y, last_col);
        }
        
        dx(last_row, last_col) = 0.0f;
        dy(last_row, last_col) = 0.0f;
    }

    ////////////////////////////////////////////////////////////
    // divergence

    struct DivergenceBody : ParallelLoopBody
    {
        void operator() (const Range& range) const;
        
        Mat_<float> v1;
        Mat_<float> v2;
        mutable Mat_<float> div;
    };

    void DivergenceBody::operator() (const Range& range) const
    {
        for (int y = range.start; y < range.end; ++y)
        {
            const float* v1Row = v1[y];
            const float* v2PrevRow = v2[y - 1];
            const float* v2CurRow = v2[y];
            
            float* divRow = div[y];
            
            for(int x = 1; x < v1.cols; ++x)
            {
                const float v1x = v1Row[x] - v1Row[x - 1];
                const float v2y = v2CurRow[x] - v2PrevRow[x];
                
                divRow[x] = v1x + v2y;
            }
        }
    }

    void divergence(const Mat_<float>& v1, const Mat_<float>& v2, Mat_<float>& div)
    {
        CV_DbgAssert( v1.rows > 2 && v1.cols > 2 );
        CV_DbgAssert( v2.size() == v1.size() );
        CV_DbgAssert( div.size() == v1.size() );
        
        {
            DivergenceBody body;
            
            body.v1 = v1;
            body.v2 = v2;
            body.div = div;
            
            parallel_for_(Range(1, v1.rows), body);
        }
        
        // compute the divergence on the first row
        for(int x = 1; x < v1.cols; ++x)
            div(0, x) = v1(0, x) - v1(0, x - 1) + v2(0, x);
        
        // compute the divergence on the first column
        for (int y = 1; y < v1.rows; ++y)
            div(y, 0) = v1(y, 0) + v2(y, 0) - v2(y - 1, 0);
        
        div(0, 0) = v1(0, 0) + v2(0, 0);
    }

    ////////////////////////////////////////////////////////////
    // calcGradRho

    struct CalcGradRhoBody : ParallelLoopBody
    {
        void operator() (const Range& range) const;
        
        Mat_<float> I0;
        Mat_<float> I1w;
        Mat_<float> I1wx;
        Mat_<float> I1wy;
        Mat_<float> u1;
        Mat_<float> u2;
        mutable Mat_<float> grad;
        mutable Mat_<float> rho_c;
    };

    void CalcGradRhoBody::operator() (const Range& range) const
    {
        for (int y = range.start; y < range.end; ++y)
        {
            const float* I0Row = I0[y];
            const float* I1wRow = I1w[y];
            const float* I1wxRow = I1wx[y];
            const float* I1wyRow = I1wy[y];
            const float* u1Row = u1[y];
            const float* u2Row = u2[y];
            
            float* gradRow = grad[y];
            float* rhoRow = rho_c[y];
            
            for (int x = 0; x < I0.cols; ++x)
            {
                const float Ix2 = I1wxRow[x] * I1wxRow[x];
                const float Iy2 = I1wyRow[x] * I1wyRow[x];
                
                // store the |Grad(I1)|^2
                gradRow[x] = Ix2 + Iy2;
                
                // compute the constant part of the rho function
                rhoRow[x] = (I1wRow[x] - I1wxRow[x] * u1Row[x] - I1wyRow[x] * u2Row[x] - I0Row[x]);
            }
        }
    }

    void calcGradRho(const Mat_<float>& I0, const Mat_<float>& I1w, const Mat_<float>& I1wx, const Mat_<float>& I1wy, const Mat_<float>& u1, const Mat_<float>& u2,
                     Mat_<float>& grad, Mat_<float>& rho_c)
    {
        CV_DbgAssert( I1w.size() == I0.size() );
        CV_DbgAssert( I1wx.size() == I0.size() );
        CV_DbgAssert( I1wy.size() == I0.size() );
        CV_DbgAssert( u1.size() == I0.size() );
        CV_DbgAssert( u2.size() == I0.size() );
        CV_DbgAssert( grad.size() == I0.size() );
        CV_DbgAssert( rho_c.size() == I0.size() );
        
        CalcGradRhoBody body;
        
        body.I0 = I0;
        body.I1w = I1w;
        body.I1wx = I1wx;
        body.I1wy = I1wy;
        body.u1 = u1;
        body.u2 = u2;
        body.grad = grad;
        body.rho_c = rho_c;
        
        parallel_for_(Range(0, I0.rows), body);
    }

    ////////////////////////////////////////////////////////////
    // estimateV

    struct EstimateVBody : ParallelLoopBody
    {
        void operator() (const Range& range) const;
        
        Mat_<float> I1wx;
        Mat_<float> I1wy;
        Mat_<float> u1;
        Mat_<float> u2;
        Mat_<float> grad;
        Mat_<float> rho_c;
        mutable Mat_<float> v1;
        mutable Mat_<float> v2;
        float l_t;
    };

    void EstimateVBody::operator() (const Range& range) const
    {
        for (int y = range.start; y < range.end; ++y)
        {
            const float* I1wxRow = I1wx[y];
            const float* I1wyRow = I1wy[y];
            const float* u1Row = u1[y];
            const float* u2Row = u2[y];
            const float* gradRow = grad[y];
            const float* rhoRow = rho_c[y];
            
            float* v1Row = v1[y];
            float* v2Row = v2[y];
            
            for (int x = 0; x < I1wx.cols; ++x)
            {
                const float rho = rhoRow[x] + (I1wxRow[x] * u1Row[x] + I1wyRow[x] * u2Row[x]);
                
                float d1 = 0.0f;
                float d2 = 0.0f;
                
                if (rho < -l_t * gradRow[x])
                {
                    d1 = l_t * I1wxRow[x];
                    d2 = l_t * I1wyRow[x];
                }
                else if (rho > l_t * gradRow[x])
                {
                    d1 = -l_t * I1wxRow[x];
                    d2 = -l_t * I1wyRow[x];
                }
                else if (gradRow[x] > std::numeric_limits<float>::epsilon())
                {
                    float fi = -rho / gradRow[x];
                    d1 = fi * I1wxRow[x];
                    d2 = fi * I1wyRow[x];
                }
                
                v1Row[x] = u1Row[x] + d1;
                v2Row[x] = u2Row[x] + d2;
            }
        }
    }

    void estimateV(const Mat_<float>& I1wx, const Mat_<float>& I1wy, const Mat_<float>& u1, const Mat_<float>& u2, const Mat_<float>& grad, const Mat_<float>& rho_c,
                   Mat_<float>& v1, Mat_<float>& v2, float l_t)
    {
        CV_DbgAssert( I1wy.size() == I1wx.size() );
        CV_DbgAssert( u1.size() == I1wx.size() );
        CV_DbgAssert( u2.size() == I1wx.size() );
        CV_DbgAssert( grad.size() == I1wx.size() );
        CV_DbgAssert( rho_c.size() == I1wx.size() );
        CV_DbgAssert( v1.size() == I1wx.size() );
        CV_DbgAssert( v2.size() == I1wx.size() );
        
        EstimateVBody body;
        
        body.I1wx = I1wx;
        body.I1wy = I1wy;
        body.u1 = u1;
        body.u2 = u2;
        body.grad = grad;
        body.rho_c = rho_c;
        body.v1 = v1;
        body.v2 = v2;
        body.l_t = l_t;
        
        parallel_for_(Range(0, I1wx.rows), body);
    }

    ////////////////////////////////////////////////////////////
    // estimateU

    float estimateU(const Mat_<float>& v1, const Mat_<float>& v2, const Mat_<float>& div_p1, const Mat_<float>& div_p2, Mat_<float>& u1, Mat_<float>& u2, float theta)
    {
        CV_DbgAssert( v2.size() == v1.size() );
        CV_DbgAssert( div_p1.size() == v1.size() );
        CV_DbgAssert( div_p2.size() == v1.size() );
        CV_DbgAssert( u1.size() == v1.size() );
        CV_DbgAssert( u2.size() == v1.size() );
        
        float error = 0.0f;
        for (int y = 0; y < v1.rows; ++y)
        {
            const float* v1Row = v1[y];
            const float* v2Row = v2[y];
            const float* divP1Row = div_p1[y];
            const float* divP2Row = div_p2[y];
            
            float* u1Row = u1[y];
            float* u2Row = u2[y];
            
            for (int x = 0; x < v1.cols; ++x)
            {
                const float u1k = u1Row[x];
                const float u2k = u2Row[x];
                
                u1Row[x] = v1Row[x] + theta * divP1Row[x];
                u2Row[x] = v2Row[x] + theta * divP2Row[x];
                
                error += (u1Row[x] - u1k) * (u1Row[x] - u1k) + (u2Row[x] - u2k) * (u2Row[x] - u2k);
            }
        }
        
        return error;
    }

    ////////////////////////////////////////////////////////////
    // estimateDualVariables

    struct EstimateDualVariablesBody : ParallelLoopBody
    {
        void operator() (const Range& range) const;
        
        Mat_<float> u1x;
        Mat_<float> u1y;
        Mat_<float> u2x;
        Mat_<float> u2y;
        mutable Mat_<float> p11;
        mutable Mat_<float> p12;
        mutable Mat_<float> p21;
        mutable Mat_<float> p22;
        float taut;
    };

    void EstimateDualVariablesBody::operator() (const Range& range) const
    {
        for (int y = range.start; y < range.end; ++y)
        {
            const float* u1xRow = u1x[y];
            const float* u1yRow = u1y[y];
            const float* u2xRow = u2x[y];
            const float* u2yRow = u2y[y];
            
            float* p11Row = p11[y];
            float* p12Row = p12[y];
            float* p21Row = p21[y];
            float* p22Row = p22[y];
            
            for (int x = 0; x < u1x.cols; ++x)
            {
                const float g1 = static_cast<float>(hypot(u1xRow[x], u1yRow[x]));
                const float g2 = static_cast<float>(hypot(u2xRow[x], u2yRow[x]));
                
                const float ng1  = 1.0f + taut * g1;
                const float ng2  = 1.0f + taut * g2;
                
                p11Row[x] = (p11Row[x] + taut * u1xRow[x]) / ng1;
                p12Row[x] = (p12Row[x] + taut * u1yRow[x]) / ng1;
                p21Row[x] = (p21Row[x] + taut * u2xRow[x]) / ng2;
                p22Row[x] = (p22Row[x] + taut * u2yRow[x]) / ng2;
            }
        }
    }

    void estimateDualVariables(const Mat_<float>& u1x, const Mat_<float>& u1y, const Mat_<float>& u2x, const Mat_<float>& u2y,
                               Mat_<float>& p11, Mat_<float>& p12, Mat_<float>& p21, Mat_<float>& p22, float taut)
    {
        CV_DbgAssert( u1y.size() == u1x.size() );
        CV_DbgAssert( u2x.size() == u1x.size() );
        CV_DbgAssert( u2y.size() == u1x.size() );
        CV_DbgAssert( p11.size() == u1x.size() );
        CV_DbgAssert( p12.size() == u1x.size() );
        CV_DbgAssert( p21.size() == u1x.size() );
        CV_DbgAssert( p22.size() == u1x.size() );
        
        EstimateDualVariablesBody body;
        
        body.u1x = u1x;
        body.u1y = u1y;
        body.u2x = u2x;
        body.u2y = u2y;
        body.p11 = p11;
        body.p12 = p12;
        body.p21 = p21;
        body.p22 = p22;
        body.taut = taut;
        
        parallel_for_(Range(0, u1x.rows), body);
    }

    void tvl1flow::procOneScale(const Mat_<float>& I0, const Mat_<float>& I1, Mat_<float>& u1, Mat_<float>& u2)
    {
        const float scaledEpsilon = static_cast<float>(epsilon * epsilon * I0.size().area());
        
        CV_DbgAssert( I1.size() == I0.size() );
        CV_DbgAssert( I1.type() == I0.type() );
        CV_DbgAssert( u1.size() == I0.size() );
        CV_DbgAssert( u2.size() == u1.size() );
        
        Mat_<float> I1x = I1x_buf(cv::Rect(0, 0, I0.cols, I0.rows));
        Mat_<float> I1y = I1y_buf(cv::Rect(0, 0, I0.cols, I0.rows));
        centeredGradient(I1, I1x, I1y);
        
        Mat_<float> flowMap1 = flowMap1_buf(cv::Rect(0, 0, I0.cols, I0.rows));
        Mat_<float> flowMap2 = flowMap2_buf(cv::Rect(0, 0, I0.cols, I0.rows));
        
        Mat_<float> I1w = I1w_buf(cv::Rect(0, 0, I0.cols, I0.rows));
        Mat_<float> I1wx = I1wx_buf(cv::Rect(0, 0, I0.cols, I0.rows));
        Mat_<float> I1wy = I1wy_buf(cv::Rect(0, 0, I0.cols, I0.rows));
        
        Mat_<float> grad = grad_buf(cv::Rect(0, 0, I0.cols, I0.rows));
        Mat_<float> rho_c = rho_c_buf(cv::Rect(0, 0, I0.cols, I0.rows));
        
        Mat_<float> v1 = v1_buf(cv::Rect(0, 0, I0.cols, I0.rows));
        Mat_<float> v2 = v2_buf(cv::Rect(0, 0, I0.cols, I0.rows));
        
        Mat_<float> p11 = p11_buf(cv::Rect(0, 0, I0.cols, I0.rows));
        Mat_<float> p12 = p12_buf(cv::Rect(0, 0, I0.cols, I0.rows));
        Mat_<float> p21 = p21_buf(cv::Rect(0, 0, I0.cols, I0.rows));
        Mat_<float> p22 = p22_buf(cv::Rect(0, 0, I0.cols, I0.rows));
        p11.setTo(Scalar::all(0));
        p12.setTo(Scalar::all(0));
        p21.setTo(Scalar::all(0));
        p22.setTo(Scalar::all(0));
        
        Mat_<float> div_p1 = div_p1_buf(cv::Rect(0, 0, I0.cols, I0.rows));
        Mat_<float> div_p2 = div_p2_buf(cv::Rect(0, 0, I0.cols, I0.rows));
        
        Mat_<float> u1x = u1x_buf(cv::Rect(0, 0, I0.cols, I0.rows));
        Mat_<float> u1y = u1y_buf(cv::Rect(0, 0, I0.cols, I0.rows));
        Mat_<float> u2x = u2x_buf(cv::Rect(0, 0, I0.cols, I0.rows));
        Mat_<float> u2y = u2y_buf(cv::Rect(0, 0, I0.cols, I0.rows));
        
        const float l_t = static_cast<float>(lambda * theta);
        const float taut = static_cast<float>(tau / theta);
        
        for (int warpings = 0; warpings < warps; ++warpings)
        {
            // compute the warping of the target image and its derivatives
            buildFlowMap(u1, u2, flowMap1, flowMap2);
            remap(I1, I1w, flowMap1, flowMap2, INTER_CUBIC);
            remap(I1x, I1wx, flowMap1, flowMap2, INTER_CUBIC);
            remap(I1y, I1wy, flowMap1, flowMap2, INTER_CUBIC);
            
            calcGradRho(I0, I1w, I1wx, I1wy, u1, u2, grad, rho_c);
            
            float error = std::numeric_limits<float>::max();
            for (int n = 0; error > scaledEpsilon && n < iterations; ++n)
            {
                // estimate the values of the variable (v1, v2) (thresholding operator TH)
                estimateV(I1wx, I1wy, u1, u2, grad, rho_c, v1, v2, l_t);
                
                // compute the divergence of the dual variable (p1, p2)
                divergence(p11, p12, div_p1);
                divergence(p21, p22, div_p2);
                
                // estimate the values of the optical flow (u1, u2)
                error = estimateU(v1, v2, div_p1, div_p2, u1, u2, static_cast<float>(theta));
                
                // compute the gradient of the optical flow (Du1, Du2)
                forwardGradient(u1, u1x, u1y);
                forwardGradient(u2, u2x, u2y);
                
                // estimate the values of the dual variable (p1, p2)
                estimateDualVariables(u1x, u1y, u2x, u2y, p11, p12, p21, p22, taut);
            }
        }
    }

    void tvl1flow::collectGarbage()
    {
        I0s.clear();
        I1s.clear();
        u1s.clear();
        u2s.clear();
        
        I1x_buf.release();
        I1y_buf.release();
        
        flowMap1_buf.release();
        flowMap2_buf.release();
        
        I1w_buf.release();
        I1wx_buf.release();
        I1wy_buf.release();
        
        grad_buf.release();
        rho_c_buf.release();
        
        v1_buf.release();
        v2_buf.release();
        
        p11_buf.release();
        p12_buf.release();
        p21_buf.release();
        p22_buf.release();
        
        div_p1_buf.release();
        div_p2_buf.release();
        
        u1x_buf.release();
        u1y_buf.release();
        u2x_buf.release();
        u2y_buf.release();
    }
}