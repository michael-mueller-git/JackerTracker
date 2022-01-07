#pragma once

#include "opencv2/tracking.hpp"
#include "opencv2/core/hal/hal.hpp"
#include "opencv2/core/ocl.hpp"

#include "Trackers.h"

using namespace cv;
using namespace cv::tracking;

namespace mycv {

class GpuTrackerKCFImpl : public TrackerJT
{
public:
    enum MODE {
        GRAY = (1 << 0),
        CN = (1 << 1),
        CUSTOM = (1 << 2)
    };

    GpuTrackerKCFImpl(const TrackerKCF::Params& parameters, TrackingTarget& target, TrackingTarget::TrackingState& state);

    virtual void initInternal(void* imagePtr);
    virtual bool updateInternal(void* imagePtr);

    TrackerKCF::Params params;

protected:
    void createHanningWindow(OutputArray dest, const cv::Size winSize, const int type) const;

    void inline fft2(const Mat src, std::vector<Mat>& dest, std::vector<Mat>& layers_data) const;
    void inline fft2(const Mat src, Mat& dest) const;
    void inline ifft2(const Mat src, Mat& dest) const;
    
    void inline cudafft2(const cuda::GpuMat src, std::vector<cuda::GpuMat>& dest, std::vector<cuda::GpuMat>& layers_data) const;
    void inline cudafft2(const cuda::GpuMat src, cuda::GpuMat& dest) const;
    void inline cudaifft2(const cuda::GpuMat src, cuda::GpuMat& dest) const;

    void inline pixelWiseMult(const std::vector<Mat> src1, const std::vector<Mat>  src2, std::vector<Mat>& dest, const int flags, const bool conjB = false) const;
    void inline sumChannels(std::vector<Mat> src, Mat& dest) const;
    void inline updateProjectionMatrix(const Mat src, Mat& old_cov, Mat& proj_matrix, float pca_rate, int compressed_sz,
        std::vector<Mat>& layers_pca, std::vector<Scalar>& average, Mat pca_data, Mat new_cov, Mat w, Mat u, Mat v);
    void inline compress(const Mat proj_matrix, const Mat src, Mat& dest, Mat& data, Mat& compressed) const;
    bool getSubWindow(const Mat img, const Rect roi, Mat& feat, Mat& patch, MODE desc = GRAY) const;
    bool getSubWindow(const Mat img, const Rect roi, Mat& feat, void (*f)(const Mat, const Rect, Mat&)) const;
    void extractCN(Mat patch_data, Mat& cnFeatures) const;
    
    void denseGaussKernel(
        const float sigma,
        const Mat x_data,
        const Mat y_data,
        Mat& k_data,

        std::vector<Mat>& xf_data,
        std::vector<Mat>& yf_data,
        std::vector<Mat> xyf_v,
        Mat xy,
        Mat xyf
    );
    
    void calcResponse(const Mat alphaf_data, const Mat kf_data, Mat& response_data, Mat& spec_data) const;
    void calcResponse(const Mat alphaf_data, const Mat alphaf_den_data, const Mat kf_data, Mat& response_data, Mat& spec_data, Mat& spec2_data) const;

    void shiftRows(Mat& mat) const;
    void shiftRows(Mat& mat, int n) const;
    void shiftCols(Mat& mat, int n) const;
    bool inline oclTransposeMM(const Mat src, float alpha, UMat& dst);

private:
    float output_sigma;
    Rect2d roi;
    Mat hann; 	//hann window filter
    Mat hann_cn; //10 dimensional hann-window filter for CN features,

    Mat y, yf; 	// training response and its FFT
    Mat x; 	// observation and its FFT
    Mat k, kf;	// dense gaussian kernel and its FFT
    Mat kf_lambda; // kf+lambda
    Mat new_alphaf, alphaf;	// training coefficients
    Mat new_alphaf_den, alphaf_den; // for splitted training coefficients
    Mat z; // model
    Mat response; // detection result
    Mat old_cov_mtx, proj_mtx; // for feature compression

    // pre-defined Mat variables for optimization of private functions
    std::vector<Mat> layers;
    Mat spec, spec2;
    std::vector<Mat> vxf, vyf, vxyf;
    Mat xy_data, xyf_data;
    Mat data_temp, compress_data;
    std::vector<Mat> layers_pca_data;
    std::vector<Scalar> average_data;
    Mat img_Patch;

    vector<cuda::GpuMat> gpu_xf_data;
    vector<cuda::GpuMat> gpu_yf_data;
    vector<cuda::GpuMat> gpu_layers;

    // storage for the extracted features, KRLS model, KRLS compressed model
    Mat X[2], Z[2], Zc[2];

    // storage of the extracted features
    std::vector<Mat> features_pca;
    std::vector<Mat> features_npca;
    std::vector<MODE> descriptors_pca;
    std::vector<MODE> descriptors_npca;

    // optimization variables for updateProjectionMatrix
    Mat data_pca, new_covar, w_data, u_data, vt_data;

    // custom feature extractor
    bool use_custom_extractor_pca;
    bool use_custom_extractor_npca;
    std::vector<void(*)(const Mat img, const Rect roi, Mat& output)> extractor_pca;
    std::vector<void(*)(const Mat img, const Rect roi, Mat& output)> extractor_npca;

    bool resizeImage; // resize the image whenever needed and the patch size is large

    ocl::Kernel transpose_mm_ker; // OCL kernel to compute transpose matrix multiply matrix.

    int frame;
};

}

namespace cv {
    namespace detail {
        inline namespace tracking {

            

            /* Cholesky decomposition
             The function performs Cholesky decomposition <https://en.wikipedia.org/wiki/Cholesky_decomposition>.
             A - the Hermitian, positive-definite matrix,
             astep - size of row in A,
             asize - number of cols and rows in A,
             L - the lower triangular matrix, A = L*Lt.
            */

            template<typename _Tp> bool
                inline callHalCholesky(_Tp* L, size_t lstep, int lsize);

            template<> bool
                inline callHalCholesky<float>(float* L, size_t lstep, int lsize)
            {
                return hal::Cholesky32f(L, lstep, lsize, NULL, 0, 0);
            }

            template<> bool
                inline callHalCholesky<double>(double* L, size_t lstep, int lsize)
            {
                return hal::Cholesky64f(L, lstep, lsize, NULL, 0, 0);
            }

            template<typename _Tp> bool
                inline choleskyDecomposition(const _Tp* A, size_t astep, int asize, _Tp* L, size_t lstep)
            {
                bool success = false;

                astep /= sizeof(_Tp);
                lstep /= sizeof(_Tp);

                for (int i = 0; i < asize; i++)
                    for (int j = 0; j <= i; j++)
                        L[i * lstep + j] = A[i * astep + j];

                success = callHalCholesky(L, lstep * sizeof(_Tp), asize);

                if (success)
                {
                    for (int i = 0; i < asize; i++)
                        for (int j = i + 1; j < asize; j++)
                            L[i * lstep + j] = 0.0;
                }

                return success;
            }

        }
    }
}  // namespace
