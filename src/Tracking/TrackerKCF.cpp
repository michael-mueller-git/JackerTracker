#include "TrackerKCF.h"
#include "featureColorName.cpp"

#include "opencl_kernels_tracking.h"
#include <opencv2/cudawarping.hpp>
#include <opencv2/cudaarithm.hpp>

#include <iostream>
#include <complex>
#include <cmath>

namespace mycv {

void MatType(cuda::GpuMat inputMat)
{
    int inttype = inputMat.type();

    std::string r, a;
    uchar depth = inttype & CV_MAT_DEPTH_MASK;
    uchar chans = 1 + (inttype >> CV_CN_SHIFT);
    switch (depth) {
    case CV_8U:  r = "8U";   a = "Mat.at<uchar>(y,x)"; break;
    case CV_8S:  r = "8S";   a = "Mat.at<schar>(y,x)"; break;
    case CV_16U: r = "16U";  a = "Mat.at<ushort>(y,x)"; break;
    case CV_16S: r = "16S";  a = "Mat.at<short>(y,x)"; break;
    case CV_32S: r = "32S";  a = "Mat.at<int>(y,x)"; break;
    case CV_32F: r = "32F";  a = "Mat.at<float>(y,x)"; break;
    case CV_64F: r = "64F";  a = "Mat.at<double>(y,x)"; break;
    case CV_32FC2: r = "32FC2";  a = "Mat.at<complex float>(y,x)"; break;
    case CV_64FC2: r = "64FC2";  a = "Mat.at<complex double>(y,x)"; break;
    default:     r = "User"; a = "Mat.at<UKNOWN>(y,x)"; break;
    }
    r += "C";
    r += (chans + '0');
    std::cout << "GpuMat is of type " << r << " and should be accessed with " << a << std::endl;
}

void MatType(Mat inputMat)
{
    int inttype = inputMat.type();

    std::string r, a;
    uchar depth = inttype & CV_MAT_DEPTH_MASK;
    uchar chans = 1 + (inttype >> CV_CN_SHIFT);
    switch (depth) {
    case CV_8U:  r = "8U";   a = "Mat.at<uchar>(y,x)"; break;
    case CV_8S:  r = "8S";   a = "Mat.at<schar>(y,x)"; break;
    case CV_16U: r = "16U";  a = "Mat.at<ushort>(y,x)"; break;
    case CV_16S: r = "16S";  a = "Mat.at<short>(y,x)"; break;
    case CV_32S: r = "32S";  a = "Mat.at<int>(y,x)"; break;
    case CV_32F: r = "32F";  a = "Mat.at<float>(y,x)"; break;
    case CV_64F: r = "64F";  a = "Mat.at<double>(y,x)"; break;
    case CV_32FC2: r = "32FC2";  a = "Mat.at<complex float>(y,x)"; break;
    case CV_64FC2: r = "64FC2";  a = "Mat.at<complex double>(y,x)"; break;
    default:     r = "User"; a = "Mat.at<UKNOWN>(y,x)"; break;
    }
    r += "C";
    r += (chans + '0');
    std::cout << "Mat is of type " << r << " and should be accessed with " << a << std::endl;

}

/*
* Constructor
*/
GpuTrackerKCFImpl::GpuTrackerKCFImpl(const TrackerKCF::Params& parameters, TrackingTarget& target, TrackingStatus& state) :
    params(parameters),
    TrackerJT(target, state, TRACKING_TYPE::TYPE_RECT, __func__, FrameVariant::GPU_RGB)
{
    resizeImage = false;
    use_custom_extractor_pca = false;
    use_custom_extractor_npca = false;

    // For update proj matrix's multiplication
    if (ocl::useOpenCL())
    {
        cv::String err;
        ocl::ProgramSource tmmSrc = ocl::tracking::tmm_oclsrc;
        ocl::Program tmmProg(tmmSrc, String(), err);
        transpose_mm_ker.create("tmm", tmmProg);
    }
}

/*
    * Initialization:
    * - creating hann window filter
    * - ROI padding
    * - creating a gaussian response for the training ground-truth
    * - perform FFT to the gaussian response
    */
void GpuTrackerKCFImpl::initGpu(cuda::GpuMat image)
{
    Rect& boundingBox = state.rect;

    frame = 0;
    roi.x = cvRound(boundingBox.x);
    roi.y = cvRound(boundingBox.y);
    roi.width = cvRound(boundingBox.width);
    roi.height = cvRound(boundingBox.height);

    //calclulate output sigma
    output_sigma = std::sqrt(static_cast<float>(roi.width * roi.height)) * params.output_sigma_factor;
    output_sigma = -0.5f / (output_sigma * output_sigma);

    //resize the ROI whenever needed
    if (params.resize && roi.width * roi.height > params.max_patch_size) {
        resizeImage = true;
        roi.x /= 2.0;
        roi.y /= 2.0;
        roi.width /= 2.0;
        roi.height /= 2.0;
    }

    // add padding to the roi
    roi.x -= roi.width / 2;
    roi.y -= roi.height / 2;
    roi.width *= 2;
    roi.height *= 2;

    // initialize the hann window filter
    createHanningWindow(hann, roi.size(), CV_32F);

    // hann window filter for CN feature
    Mat _layer[] = { hann, hann, hann, hann, hann, hann, hann, hann, hann, hann };
    merge(_layer, 10, hann_cn);

    // create gaussian response
    y = Mat::zeros((int)roi.height, (int)roi.width, CV_32F);
    for (int i = 0; i<int(roi.height); i++) {
        for (int j = 0; j<int(roi.width); j++) {
            y.at<float>(i, j) =
                static_cast<float>((i - roi.height / 2 + 1) * (i - roi.height / 2 + 1) + (j - roi.width / 2 + 1) * (j - roi.width / 2 + 1));
        }
    }

    y *= (float)output_sigma;
    cv::exp(y, y);

    //cuda::GpuMat tmp1, tmp2;
    //tmp1.upload(y);
    //fft2(tmp1, tmp2);
    //tmp2.download(yf);
    fft2(y, yf);

    if (image.channels() == 1) { // disable CN for grayscale images
        params.desc_pca &= ~(CN);
        params.desc_npca &= ~(CN);
    }

    // record the non-compressed descriptors
    if ((params.desc_npca & GRAY) == GRAY)descriptors_npca.push_back(GRAY);
    if ((params.desc_npca & CN) == CN)descriptors_npca.push_back(CN);
    if (use_custom_extractor_npca)descriptors_npca.push_back(CUSTOM);
    features_npca.resize(descriptors_npca.size());

    // record the compressed descriptors
    if ((params.desc_pca & GRAY) == GRAY)descriptors_pca.push_back(GRAY);
    if ((params.desc_pca & CN) == CN)descriptors_pca.push_back(CN);
    if (use_custom_extractor_pca)descriptors_pca.push_back(CUSTOM);
    features_pca.resize(descriptors_pca.size());

    // accept only the available descriptor modes
    CV_Assert(
        (params.desc_pca & GRAY) == GRAY
        || (params.desc_npca & GRAY) == GRAY
        || (params.desc_pca & CN) == CN
        || (params.desc_npca & CN) == CN
        || use_custom_extractor_pca
        || use_custom_extractor_npca
    );

    // ensure roi has intersection with the image
    Rect2d image_roi(0, 0,
        image.cols / (resizeImage ? 2 : 1),
        image.rows / (resizeImage ? 2 : 1));
    CV_Assert(!(roi & image_roi).empty());
}

/*
    * Main part of the KCF algorithm
    */
bool GpuTrackerKCFImpl::updateGpu(cuda::GpuMat gpuImage)
{
    double minVal, maxVal;	// min-max response
    Point minLoc, maxLoc;	// min-max location

    CV_Assert(gpuImage.channels() == 1 || gpuImage.channels() == 3);

    cuda::GpuMat gpuImageResized;
    // resize the image whenever needed
    if (resizeImage)
        cuda::resize(gpuImage, gpuImageResized, Size(gpuImage.cols / 2, gpuImage.rows / 2), 0, 0, INTER_LINEAR);
    else
        gpuImage.copyTo(gpuImageResized);

    Mat img(gpuImageResized);

    // detection part
    if (frame > 0) {

        // extract and pre-process the patch
        // get non compressed descriptors
        for (unsigned i = 0; i < descriptors_npca.size() - extractor_npca.size(); i++) {
            if (!getSubWindow(img, roi, features_npca[i], img_Patch, descriptors_npca[i]))return false;
        }
        //get non-compressed custom descriptors
        for (unsigned i = 0, j = (unsigned)(descriptors_npca.size() - extractor_npca.size()); i < extractor_npca.size(); i++, j++) {
            if (!getSubWindow(img, roi, features_npca[j], extractor_npca[i]))return false;
        }
        if (features_npca.size() > 0)merge(features_npca, X[1]);

        // get compressed descriptors
        for (unsigned i = 0; i < descriptors_pca.size() - extractor_pca.size(); i++) {
            if (!getSubWindow(img, roi, features_pca[i], img_Patch, descriptors_pca[i]))return false;
        }
        //get compressed custom descriptors
        for (unsigned i = 0, j = (unsigned)(descriptors_pca.size() - extractor_pca.size()); i < extractor_pca.size(); i++, j++) {
            if (!getSubWindow(img, roi, features_pca[j], extractor_pca[i]))return false;
        }
        if (features_pca.size() > 0)merge(features_pca, X[0]);

        //compress the features and the KRSL model
        if (params.desc_pca != 0) {
            compress(proj_mtx, X[0], X[0], data_temp, compress_data);
            compress(proj_mtx, Z[0], Zc[0], data_temp, compress_data);
        }

        // copy the compressed KRLS model
        Zc[1] = Z[1];

        // merge all features
        if (features_npca.size() == 0) {
            x = X[0];
            z = Zc[0];
        }
        else if (features_pca.size() == 0) {
            x = X[1];
            z = Z[1];
        }
        else {
            merge(X, 2, x);
            merge(Zc, 2, z);
        }

        //compute the gaussian kernel
        denseGaussKernel(params.sigma, x, z, k, vxf, vyf, vxyf, xy_data, xyf_data);

        //fft2(k, kf);
        cuda::GpuMat tmp1, tmp2;
        tmp1.upload(k);
        cudafft2(tmp1, tmp2);
        tmp2.download(kf);
        
        if (frame == 1)spec2 = Mat_<Vec2f >(kf.rows, kf.cols);

        // calculate filter response
        if (params.split_coeff)
            calcResponse(alphaf, alphaf_den, kf, response, spec, spec2);
        else
            calcResponse(alphaf, kf, response, spec);

        // extract the maximum response
        minMaxLoc(response, &minVal, &maxVal, &minLoc, &maxLoc);
        if (maxVal < params.detect_thresh)
        {
            return false;
        }
        roi.x += (maxLoc.x - roi.width / 2 + 1);
        roi.y += (maxLoc.y - roi.height / 2 + 1);
    }

    // update the bounding box
    Rect2d boundingBox;
    boundingBox.x = (resizeImage ? roi.x * 2 : roi.x) + (resizeImage ? roi.width * 2 : roi.width) / 4;
    boundingBox.y = (resizeImage ? roi.y * 2 : roi.y) + (resizeImage ? roi.height * 2 : roi.height) / 4;
    boundingBox.width = (resizeImage ? roi.width * 2 : roi.width) / 2;
    boundingBox.height = (resizeImage ? roi.height * 2 : roi.height) / 2;

    // extract the patch for learning purpose
    // get non compressed descriptors
    for (unsigned i = 0; i < descriptors_npca.size() - extractor_npca.size(); i++) {
        if (!getSubWindow(img, roi, features_npca[i], img_Patch, descriptors_npca[i]))return false;
    }
    //get non-compressed custom descriptors
    for (unsigned i = 0, j = (unsigned)(descriptors_npca.size() - extractor_npca.size()); i < extractor_npca.size(); i++, j++) {
        if (!getSubWindow(img, roi, features_npca[j], extractor_npca[i]))return false;
    }
    if (features_npca.size() > 0)merge(features_npca, X[1]);

    // get compressed descriptors
    for (unsigned i = 0; i < descriptors_pca.size() - extractor_pca.size(); i++) {
        if (!getSubWindow(img, roi, features_pca[i], img_Patch, descriptors_pca[i]))return false;
    }
    //get compressed custom descriptors
    for (unsigned i = 0, j = (unsigned)(descriptors_pca.size() - extractor_pca.size()); i < extractor_pca.size(); i++, j++) {
        if (!getSubWindow(img, roi, features_pca[j], extractor_pca[i]))return false;
    }
    if (features_pca.size() > 0)merge(features_pca, X[0]);

    //update the training data
    if (frame == 0) {
        Z[0] = X[0].clone();
        Z[1] = X[1].clone();
    }
    else {
        Z[0] = (1.0 - params.interp_factor) * Z[0] + params.interp_factor * X[0];
        Z[1] = (1.0 - params.interp_factor) * Z[1] + params.interp_factor * X[1];
    }

    if (params.desc_pca != 0 || use_custom_extractor_pca) {
        // initialize the vector of Mat variables
        if (frame == 0) {
            layers_pca_data.resize(Z[0].channels());
            average_data.resize(Z[0].channels());
        }

        // feature compression
        updateProjectionMatrix(Z[0], old_cov_mtx, proj_mtx, params.pca_learning_rate, params.compressed_size, layers_pca_data, average_data, data_pca, new_covar, w_data, u_data, vt_data);
        compress(proj_mtx, X[0], X[0], data_temp, compress_data);
    }

    // merge all features
    if (features_npca.size() == 0)
        x = X[0];
    else if (features_pca.size() == 0)
        x = X[1];
    else
        merge(X, 2, x);

    // initialize some required Mat variables
    if (frame == 0) {
        layers.resize(x.channels());
        vxf.resize(x.channels());
        vyf.resize(x.channels());
        vxyf.resize(vyf.size());
        new_alphaf = Mat_<Vec2f >(yf.rows, yf.cols);

        gpu_xf_data.resize(x.channels());
        gpu_yf_data.resize(x.channels());
        gpu_layers.resize(x.channels());
    }

    // Kernel Regularized Least-Squares, calculate alphas
    denseGaussKernel(params.sigma, x, x, k, vxf, vyf, vxyf, xy_data, xyf_data);
    
    //cuda::GpuMat tmp1, tmp2;
    //tmp1.upload(k);
    //cudafft2(tmp1, tmp2);
    //tmp2.download(kf);

    fft2(k, kf);
    kf_lambda = kf + params.lambda;

    float den;
    if (params.split_coeff) {

        cuda::GpuMat gpu_yf, gpu_kf, gpu_new_alphaf, gpu_kf_lambda, gpu_new_alphaf_den;
        gpu_yf.upload(yf);
        gpu_kf.upload(kf);
        cuda::mulSpectrums(gpu_yf, gpu_kf, gpu_new_alphaf, 0);
        gpu_new_alphaf.download(new_alphaf);

        gpu_kf_lambda.upload(kf_lambda);
        cuda::mulSpectrums(gpu_kf, gpu_kf_lambda, gpu_new_alphaf_den, 0);
        gpu_new_alphaf_den.download(new_alphaf_den);
        
        //mulSpectrums(yf, kf, new_alphaf, 0);
        //mulSpectrums(kf, kf_lambda, new_alphaf_den, 0);
    }
    else {
        for (int i = 0; i < yf.rows; i++) {
            for (int j = 0; j < yf.cols; j++) {
                den = 1.0f / (kf_lambda.at<Vec2f>(i, j)[0] * kf_lambda.at<Vec2f>(i, j)[0] + kf_lambda.at<Vec2f>(i, j)[1] * kf_lambda.at<Vec2f>(i, j)[1]);

                new_alphaf.at<Vec2f>(i, j)[0] =
                    (yf.at<Vec2f>(i, j)[0] * kf_lambda.at<Vec2f>(i, j)[0] + yf.at<Vec2f>(i, j)[1] * kf_lambda.at<Vec2f>(i, j)[1]) * den;
                new_alphaf.at<Vec2f>(i, j)[1] =
                    (yf.at<Vec2f>(i, j)[1] * kf_lambda.at<Vec2f>(i, j)[0] - yf.at<Vec2f>(i, j)[0] * kf_lambda.at<Vec2f>(i, j)[1]) * den;
            }
        }
    }

    // update the RLS model
    if (frame == 0) {
        alphaf = new_alphaf.clone();
        if (params.split_coeff)alphaf_den = new_alphaf_den.clone();
    }
    else {
        alphaf = (1.0 - params.interp_factor) * alphaf + params.interp_factor * new_alphaf;
        if (params.split_coeff)alphaf_den = (1.0 - params.interp_factor) * alphaf_den + params.interp_factor * new_alphaf_den;
    }

    frame++;

    int x1 = cvRound(boundingBox.x);
    int y1 = cvRound(boundingBox.y);
    int x2 = cvRound(boundingBox.x + boundingBox.width);
    int y2 = cvRound(boundingBox.y + boundingBox.height);
    state.rect = Rect(x1, y1, x2 - x1, y2 - y1) & Rect(Point(0, 0), gpuImage.size());

    return true;
}


/*-------------------------------------
|  implementation of the KCF functions
|-------------------------------------*/

/*
    * hann window filter
    */
void GpuTrackerKCFImpl::createHanningWindow(OutputArray dest, const cv::Size winSize, const int type) const {
    CV_Assert(type == CV_32FC1 || type == CV_64FC1);

    dest.create(winSize, type);
    Mat dst = dest.getMat();

    int rows = dst.rows, cols = dst.cols;

    AutoBuffer<float> _wc(cols);
    float* const wc = _wc.data();

    const float coeff0 = 2.0f * (float)CV_PI / (cols - 1);
    const float coeff1 = 2.0f * (float)CV_PI / (rows - 1);
    for (int j = 0; j < cols; j++)
        wc[j] = 0.5f * (1.0f - cos(coeff0 * j));

    if (dst.depth() == CV_32F) {
        for (int i = 0; i < rows; i++) {
            float* dstData = dst.ptr<float>(i);
            float wr = 0.5f * (1.0f - cos(coeff1 * i));
            for (int j = 0; j < cols; j++)
                dstData[j] = (float)(wr * wc[j]);
        }
    }
    else {
        for (int i = 0; i < rows; i++) {
            double* dstData = dst.ptr<double>(i);
            double wr = 0.5f * (1.0f - cos(coeff1 * i));
            for (int j = 0; j < cols; j++)
                dstData[j] = wr * wc[j];
        }
    }
}

void inline GpuTrackerKCFImpl::fft2(const Mat src, Mat& dest) const {
    MatType(src);
    
    dft(src, dest, DFT_COMPLEX_OUTPUT);

    MatType(dest);

    for (int x = 0; x < (dest.cols); x++)
    {
        for (int y = 0; y < (dest.rows); y++)
        {
            Point2f p = dest.at<Point2f>(y, x);
            bool t = true;

        }
    }

}

void inline GpuTrackerKCFImpl::fft2(const Mat src, std::vector<Mat>& dest, std::vector<Mat>& layers_data) const {
    split(src, layers_data);

    for (int i = 0; i < src.channels(); i++) {
        dft(layers_data[i], dest[i], DFT_COMPLEX_OUTPUT);
    }
}

void inline GpuTrackerKCFImpl::ifft2(const Mat src, Mat& dest) const {
    idft(src, dest, DFT_SCALE + DFT_REAL_OUTPUT);
}

void inline GpuTrackerKCFImpl::cudafft2(const cuda::GpuMat src, cuda::GpuMat& dest) const {
    MatType(src);

    cuda::dft(src, dest, src.size());

    MatType(dest);
    Mat dl(dest);

    for (int x = 0; x < dl.cols; x++)
    {
        for (int y = 0; y < dl.rows; y++)
        {
            Point2f p = dl.at<Point2f>(y, x);
            bool t = true;
        }
    }
}

void inline GpuTrackerKCFImpl::cudafft2(const cuda::GpuMat src, std::vector<cuda::GpuMat>& dest, std::vector<cuda::GpuMat>& layers_data) const {
    cuda::split(src, layers_data);

    for (int i = 0; i < src.channels(); i++) {
        cuda::dft(layers_data[i], dest[i], layers_data[i].size());
    }
}

void inline GpuTrackerKCFImpl::cudaifft2(const cuda::GpuMat src, cuda::GpuMat& dest) const {
    cuda::dft(src, dest, src.size(), DFT_SCALE | DFT_REAL_OUTPUT | DFT_INVERSE);
}

/*
    * Point-wise multiplication of two Multichannel Mat data
    */
void inline GpuTrackerKCFImpl::pixelWiseMult(const std::vector<Mat> src1, const std::vector<Mat>  src2, std::vector<Mat>& dest, const int flags, const bool conjB) const {
    for (unsigned i = 0; i < src1.size(); i++) {
        mulSpectrums(src1[i], src2[i], dest[i], flags, conjB);
    }
}

/*
    * Combines all channels in a multi-channels Mat data into a single channel
    */
void inline GpuTrackerKCFImpl::sumChannels(std::vector<Mat> src, Mat& dest) const {
    dest = src[0].clone();
    for (unsigned i = 1; i < src.size(); i++) {
        dest += src[i];
    }
}

bool inline GpuTrackerKCFImpl::oclTransposeMM(const Mat src, float alpha, UMat& dst) {
    // Current kernel only support matrix's rows is multiple of 4.
    // And if one line is less than 512KB, CPU will likely be faster.
    if (transpose_mm_ker.empty() ||
        src.rows % 4 != 0 ||
        (src.rows * 10) < (1024 * 1024 / 4))
        return false;

    Size s(src.rows, src.cols);
    const Mat tmp = src.t();
    const UMat uSrc = tmp.getUMat(ACCESS_READ);
    transpose_mm_ker.args(
        ocl::KernelArg::PtrReadOnly(uSrc),
        (int)uSrc.rows,
        (int)uSrc.cols,
        alpha,
        ocl::KernelArg::PtrWriteOnly(dst));
    size_t globSize[2] = { static_cast<size_t>(src.cols * 64), static_cast<size_t>(src.cols) };
    size_t localSize[2] = { 64, 1 };
    if (!transpose_mm_ker.run(2, globSize, localSize, true))
        return false;
    return true;
}

/*
    * obtains the projection matrix using PCA
    */
void inline GpuTrackerKCFImpl::updateProjectionMatrix(const Mat src, Mat& old_cov, Mat& proj_matrix, float pca_rate, int compressed_sz,
    std::vector<Mat>& layers_pca, std::vector<Scalar>& average, Mat pca_data, Mat new_cov, Mat w, Mat u, Mat vt) {
    CV_Assert(compressed_sz <= src.channels());

    split(src, layers_pca);

    for (int i = 0; i < src.channels(); i++) {
        average[i] = mean(layers_pca[i]);
        layers_pca[i] -= average[i];
    }

    // calc covariance matrix
    merge(layers_pca, pca_data);
    pca_data = pca_data.reshape(1, src.rows * src.cols);

    bool oclSucceed = false;
    Size s(pca_data.cols, pca_data.cols);
    UMat result(s, pca_data.type());
    if (oclTransposeMM(pca_data, 1.0f / (float)(src.rows * src.cols - 1), result)) {
        if (old_cov.rows == 0) old_cov = result.getMat(ACCESS_READ).clone();
        SVD::compute((1.0 - pca_rate) * old_cov + pca_rate * result.getMat(ACCESS_READ), w, u, vt);
        oclSucceed = true;
    }
#define TMM_VERIFICATION 0

    if (oclSucceed == false || TMM_VERIFICATION) {
        new_cov = 1.0f / (float)(src.rows * src.cols - 1) * (pca_data.t() * pca_data);
#if TMM_VERIFICATION
        for (int i = 0; i < new_cov.rows; i++)
            for (int j = 0; j < new_cov.cols; j++)
                if (abs(new_cov.at<float>(i, j) - result.getMat(ACCESS_RW).at<float>(i, j)) > abs(new_cov.at<float>(i, j)) * 1e-3)
                    printf("error @ i %d j %d got %G expected %G \n", i, j, result.getMat(ACCESS_RW).at<float>(i, j), new_cov.at<float>(i, j));
#endif
        if (old_cov.rows == 0)old_cov = new_cov.clone();
        SVD::compute((1.0f - pca_rate) * old_cov + pca_rate * new_cov, w, u, vt);
    }

    // extract the projection matrix
    proj_matrix = u(Rect(0, 0, compressed_sz, src.channels())).clone();
    Mat proj_vars = Mat::eye(compressed_sz, compressed_sz, proj_matrix.type());
    for (int i = 0; i < compressed_sz; i++) {
        proj_vars.at<float>(i, i) = w.at<float>(i);
    }

    // update the covariance matrix
    old_cov = (1.0 - pca_rate) * old_cov + pca_rate * proj_matrix * proj_vars * proj_matrix.t();
}

/*
    * compress the features
    */
void inline GpuTrackerKCFImpl::compress(const Mat proj_matrix, const Mat src, Mat& dest, Mat& data, Mat& compressed) const {
    data = src.reshape(1, src.rows * src.cols);
    compressed = data * proj_matrix;
    dest = compressed.reshape(proj_matrix.cols, src.rows).clone();
}

/*
    * obtain the patch and apply hann window filter to it
    */
bool GpuTrackerKCFImpl::getSubWindow(const Mat img, const Rect _roi, Mat& feat, Mat& patch, MODE desc) const {

    Rect region = _roi;

    // return false if roi is outside the image
    if ((roi & Rect2d(0, 0, img.cols, img.rows)).empty())
        return false;

    // extract patch inside the image
    if (_roi.x < 0) { region.x = 0; region.width += _roi.x; }
    if (_roi.y < 0) { region.y = 0; region.height += _roi.y; }
    if (_roi.x + _roi.width > img.cols)region.width = img.cols - _roi.x;
    if (_roi.y + _roi.height > img.rows)region.height = img.rows - _roi.y;
    if (region.width > img.cols)region.width = img.cols;
    if (region.height > img.rows)region.height = img.rows;

    // return false if region is empty
    if (region.empty())
        return false;

    patch = img(region).clone();

    // add some padding to compensate when the patch is outside image border
    int addTop, addBottom, addLeft, addRight;
    addTop = region.y - _roi.y;
    addBottom = (_roi.height + _roi.y > img.rows ? _roi.height + _roi.y - img.rows : 0);
    addLeft = region.x - _roi.x;
    addRight = (_roi.width + _roi.x > img.cols ? _roi.width + _roi.x - img.cols : 0);

    copyMakeBorder(patch, patch, addTop, addBottom, addLeft, addRight, BORDER_REPLICATE);
    if (patch.rows == 0 || patch.cols == 0)return false;

    // extract the desired descriptors
    switch (desc) {
    case CN:
        CV_Assert(img.channels() == 3);
        extractCN(patch, feat);
        feat = feat.mul(hann_cn); // hann window filter
        break;
    default: // GRAY
        if (img.channels() > 1)
            cvtColor(patch, feat, COLOR_BGR2GRAY);
        else
            feat = patch;
        //feat.convertTo(feat,CV_32F);
        feat.convertTo(feat, CV_32F, 1.0 / 255.0, -0.5);
        //feat=feat/255.0-0.5; // normalize to range -0.5 .. 0.5
        feat = feat.mul(hann); // hann window filter
        break;
    }

    return true;

}

/*
    * get feature using external function
    */
bool GpuTrackerKCFImpl::getSubWindow(const Mat img, const Rect _roi, Mat& feat, void (*f)(const Mat, const Rect, Mat&)) const {

    // return false if roi is outside the image
    if ((_roi.x + _roi.width < 0)
        || (_roi.y + _roi.height < 0)
        || (_roi.x >= img.cols)
        || (_roi.y >= img.rows)
        )return false;

    f(img, _roi, feat);

    if (_roi.width != feat.cols || _roi.height != feat.rows) {
        printf("error in customized function of features extractor!\n");
        printf("Rules: roi.width==feat.cols && roi.height = feat.rows \n");
    }

    Mat hann_win;
    std::vector<Mat> _layers;

    for (int i = 0; i < feat.channels(); i++)
        _layers.push_back(hann);

    merge(_layers, hann_win);

    feat = feat.mul(hann_win); // hann window filter

    return true;
}

/* Convert BGR to ColorNames
    */
void GpuTrackerKCFImpl::extractCN(Mat patch_data, Mat& cnFeatures) const {
    Vec3b& pixel = patch_data.at<Vec3b>(0, 0);
    unsigned index;

    if (cnFeatures.type() != CV_32FC(10))
        cnFeatures = Mat::zeros(patch_data.rows, patch_data.cols, CV_32FC(10));

    for (int i = 0; i < patch_data.rows; i++) {
        for (int j = 0; j < patch_data.cols; j++) {
            pixel = patch_data.at<Vec3b>(i, j);
            index = (unsigned)(floor((float)pixel[2] / 8) + 32 * floor((float)pixel[1] / 8) + 32 * 32 * floor((float)pixel[0] / 8));

            //copy the values
            for (int _k = 0; _k < 10; _k++) {
                cnFeatures.at<Vec<float, 10> >(i, j)[_k] = ColorNames[index][_k];
            }
        }
    }

}

/*
    *  dense gauss kernel function
    */
void GpuTrackerKCFImpl::denseGaussKernel(
    const float sigma,
    const Mat x_data,
    const Mat y_data,
    Mat& k_data,

    std::vector<Mat>& xf_data,
    std::vector<Mat>& yf_data,
    std::vector<Mat> xyf_v,
    Mat xy,
    Mat xyf
) {
    double normX, normY;

    /*
    cuda::GpuMat gpu_x_dat, gpu_y_dat;
    gpu_x_dat.upload(x_data);
    gpu_y_dat.upload(y_data);

    fft2(gpu_x_dat, gpu_xf_data, gpu_layers_data);
    fft2(gpu_y_dat, gpu_yf_data, gpu_layers_data);

    for (int i = 0; i < x_data.channels(); i++)
    {
        gpu_xf_data[i].download(xf_data[i]);
        gpu_yf_data[i].download(yf_data[i]);
    }
    */

    fft2(x_data, xf_data, layers);
    fft2(y_data, yf_data, layers);

    normX = norm(x_data);
    normX *= normX;
    normY = norm(y_data);
    normY *= normY;

    pixelWiseMult(xf_data, yf_data, xyf_v, 0, true);
    sumChannels(xyf_v, xyf);

    ifft2(xyf, xyf);
    
    //cuda::GpuMat tmp1, tmp2;
    //tmp1.upload(xyf);
    //cudaifft2(tmp1, tmp2);
    //tmp2.download(xyf);

    if (params.wrap_kernel) {
        shiftRows(xyf, x_data.rows / 2);
        shiftCols(xyf, x_data.cols / 2);
    }

    //(xx + yy - 2 * xy) / numel(x)
    xy = (normX + normY - 2 * xyf) / (x_data.rows * x_data.cols * x_data.channels());

    // TODO: check wether we really need thresholding or not
    //threshold(xy,xy,0.0,0.0,THRESH_TOZERO);//max(0, (xx + yy - 2 * xy) / numel(x))
    for (int i = 0; i < xy.rows; i++) {
        for (int j = 0; j < xy.cols; j++) {
            if (xy.at<float>(i, j) < 0.0)xy.at<float>(i, j) = 0.0;
        }
    }

    float sig = -1.0f / (sigma * sigma);
    xy = sig * xy;
    exp(xy, k_data);

}

/* CIRCULAR SHIFT Function
    * http://stackoverflow.com/questions/10420454/shift-like-matlab-function-rows-or-columns-of-a-matrix-in-opencv
    */
    // circular shift one row from up to down
void GpuTrackerKCFImpl::shiftRows(Mat& mat) const {

    Mat temp;
    Mat m;
    int _k = (mat.rows - 1);
    mat.row(_k).copyTo(temp);
    for (; _k > 0; _k--) {
        m = mat.row(_k);
        mat.row(_k - 1).copyTo(m);
    }
    m = mat.row(0);
    temp.copyTo(m);

}

// circular shift n rows from up to down if n > 0, -n rows from down to up if n < 0
void GpuTrackerKCFImpl::shiftRows(Mat& mat, int n) const {
    if (n < 0) {
        n = -n;
        flip(mat, mat, 0);
        for (int _k = 0; _k < n; _k++) {
            shiftRows(mat);
        }
        flip(mat, mat, 0);
    }
    else {
        for (int _k = 0; _k < n; _k++) {
            shiftRows(mat);
        }
    }
}

//circular shift n columns from left to right if n > 0, -n columns from right to left if n < 0
void GpuTrackerKCFImpl::shiftCols(Mat& mat, int n) const {
    if (n < 0) {
        n = -n;
        flip(mat, mat, 1);
        transpose(mat, mat);
        shiftRows(mat, n);
        transpose(mat, mat);
        flip(mat, mat, 1);
    }
    else {
        transpose(mat, mat);
        shiftRows(mat, n);
        transpose(mat, mat);
    }
}

/*
    * calculate the detection response
    */
void GpuTrackerKCFImpl::calcResponse(const Mat alphaf_data, const Mat kf_data, Mat& response_data, Mat& spec_data) const {
    //alpha f--> 2channels ; k --> 1 channel;
    mulSpectrums(alphaf_data, kf_data, spec_data, 0, false);


    ifft2(spec_data, response_data);

    //cuda::GpuMat tmp1, tmp2;
    //tmp1.upload(spec_data);
    //cudaifft2(tmp1, tmp2);
    //tmp2.download(response_data);
}

/*
    * calculate the detection response for splitted form
    */
void GpuTrackerKCFImpl::calcResponse(const Mat alphaf_data, const Mat _alphaf_den, const Mat kf_data, Mat& response_data, Mat& spec_data, Mat& spec2_data) const {

    mulSpectrums(alphaf_data, kf_data, spec_data, 0, false);

    //z=(a+bi)/(c+di)=[(ac+bd)+i(bc-ad)]/(c^2+d^2)
    float den;
    for (int i = 0; i < kf_data.rows; i++) {
        for (int j = 0; j < kf_data.cols; j++) {
            den = 1.0f / (_alphaf_den.at<Vec2f>(i, j)[0] * _alphaf_den.at<Vec2f>(i, j)[0] + _alphaf_den.at<Vec2f>(i, j)[1] * _alphaf_den.at<Vec2f>(i, j)[1]);
            spec2_data.at<Vec2f>(i, j)[0] =
                (spec_data.at<Vec2f>(i, j)[0] * _alphaf_den.at<Vec2f>(i, j)[0] + spec_data.at<Vec2f>(i, j)[1] * _alphaf_den.at<Vec2f>(i, j)[1]) * den;
            spec2_data.at<Vec2f>(i, j)[1] =
                (spec_data.at<Vec2f>(i, j)[1] * _alphaf_den.at<Vec2f>(i, j)[0] - spec_data.at<Vec2f>(i, j)[0] * _alphaf_den.at<Vec2f>(i, j)[1]) * den;
        }
    }

    ifft2(spec2_data, response_data);

    /*
    cuda::GpuMat tmp1, tmp2;
    tmp1.upload(spec2_data);
    ifft2(tmp1, tmp2);
    tmp2.download(response_data);
    */
}

}