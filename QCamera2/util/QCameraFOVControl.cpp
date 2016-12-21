/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above
*       copyright notice, this list of conditions and the following
*       disclaimer in the documentation and/or other materials provided
*       with the distribution.
*     * Neither the name of The Linux Foundation nor the names of its
*       contributors may be used to endorse or promote products derived
*       from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
* ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
*/

#define LOG_TAG "QCameraFOVControl"

#include <utils/Errors.h>
#include "QCameraFOVControl.h"
#include "QCameraFOVControlSettings.h"

extern "C" {
#include "mm_camera_dbg.h"
}

namespace qcamera {

/*===========================================================================
 * FUNCTION   : QCameraFOVControl constructor
 *
 * DESCRIPTION: class constructor
 *
 * PARAMETERS : none
 *
 * RETURN     : void
 *
 *==========================================================================*/
QCameraFOVControl::QCameraFOVControl()
{
    memset(&mDualCamParams,    0, sizeof(dual_cam_params_t));
    memset(&mFovControlConfig, 0, sizeof(fov_control_config_t));
    memset(&mFovControlData,   0, sizeof(fov_control_data_t));
    memset(&mFovControlResult, 0, sizeof(fov_control_result_t));
}


/*===========================================================================
 * FUNCTION   : QCameraFOVControl destructor
 *
 * DESCRIPTION: class destructor
 *
 * PARAMETERS : none
 *
 * RETURN     : void
 *
 *==========================================================================*/
QCameraFOVControl::~QCameraFOVControl()
{
}


/*===========================================================================
 * FUNCTION   : create
 *
 * DESCRIPTION: This is a static method to create FOV-control object. It calls
 *              private constructor of the class and only returns a valid object
 *              if it can successfully initialize the FOV-control.
 *
 * PARAMETERS :
 *  @capsMain : The capabilities for the main camera
 *  @capsAux  : The capabilities for the aux camera
 *
 * RETURN     : Valid object pointer if succeeds
 *              NULL if fails
 *
 *==========================================================================*/
QCameraFOVControl* QCameraFOVControl::create(
        cam_capability_t *capsMainCam,
        cam_capability_t *capsAuxCam)
{
    QCameraFOVControl *pFovControl  = NULL;

    if (capsMainCam && capsAuxCam) {
        // Create FOV control object
        pFovControl = new QCameraFOVControl();

        if (pFovControl) {
            bool  success = false;
            if (pFovControl->validateAndExtractParameters(capsMainCam, capsAuxCam)) {
                // Based on focal lengths, map main and aux camera to wide and tele
                if (pFovControl->mDualCamParams.paramsMain.focalLengthMm <
                    pFovControl->mDualCamParams.paramsAux.focalLengthMm) {
                    pFovControl->mFovControlData.camWide  = CAM_TYPE_MAIN;
                    pFovControl->mFovControlData.camTele  = CAM_TYPE_AUX;
                    pFovControl->mFovControlData.camState = STATE_WIDE;
                } else {
                    pFovControl->mFovControlData.camWide  = CAM_TYPE_AUX;
                    pFovControl->mFovControlData.camTele  = CAM_TYPE_MAIN;
                    pFovControl->mFovControlData.camState = STATE_TELE;
                }

                // Initialize the master info to main camera
                pFovControl->mFovControlResult.camMasterPreview  = CAM_TYPE_MAIN;
                pFovControl->mFovControlResult.camMaster3A       = CAM_TYPE_MAIN;
                success = true;
            }

            if (!success) {
                LOGE("FOV-control: Failed to create an object");
                delete pFovControl;
                pFovControl = NULL;
            }
        } else {
            LOGE("FOV-control: Failed to allocate memory for FOV-control object");
        }
    }

    return pFovControl;
}


/*===========================================================================
 * FUNCTION    : consolidateCapabilities
 *
 * DESCRIPTION : Combine the capabilities from main and aux cameras to return
 *               the consolidated capabilities.
 *
 * PARAMETERS  :
 * @capsMainCam: Capabilities for the main camera
 * @capsAuxCam : Capabilities for the aux camera
 *
 * RETURN      : Consolidated capabilities
 *
 *==========================================================================*/
cam_capability_t QCameraFOVControl::consolidateCapabilities(
        cam_capability_t *capsMainCam,
        cam_capability_t *capsAuxCam)
{
    cam_capability_t capsConsolidated;

    if ((capsMainCam != NULL) &&
        (capsAuxCam  != NULL)) {

        memcpy(&capsConsolidated, capsMainCam, sizeof(cam_capability_t));

        // Consolidate preview sizes
        uint32_t previewSizesTblCntMain  = capsMainCam->preview_sizes_tbl_cnt;
        uint32_t previewSizesTblCntAux   = capsAuxCam->preview_sizes_tbl_cnt;
        uint32_t previewSizesTblCntFinal = 0;

        for (uint32_t i = 0; i < previewSizesTblCntMain; ++i) {
            for (uint32_t j = 0; j < previewSizesTblCntAux; ++j) {
                if ((capsMainCam->preview_sizes_tbl[i].width ==
                     capsAuxCam->preview_sizes_tbl[j].width) &&
                    (capsMainCam->preview_sizes_tbl[i].height ==
                     capsAuxCam->preview_sizes_tbl[j].height)) {
                    if (previewSizesTblCntFinal != i) {
                        capsConsolidated.preview_sizes_tbl[previewSizesTblCntFinal].width =
                           capsAuxCam->preview_sizes_tbl[j].width;
                        capsConsolidated.preview_sizes_tbl[previewSizesTblCntFinal].height =
                           capsMainCam->preview_sizes_tbl[j].height;
                    }
                    ++previewSizesTblCntFinal;
                    break;
                }
            }
        }
        capsConsolidated.preview_sizes_tbl_cnt = previewSizesTblCntFinal;

        // Consolidate video sizes
        uint32_t videoSizesTblCntMain  = capsMainCam->video_sizes_tbl_cnt;
        uint32_t videoSizesTblCntAux   = capsAuxCam->video_sizes_tbl_cnt;
        uint32_t videoSizesTblCntFinal = 0;

        for (uint32_t i = 0; i < videoSizesTblCntMain; ++i) {
            for (uint32_t j = 0; j < videoSizesTblCntAux; ++j) {
                if ((capsMainCam->video_sizes_tbl[i].width ==
                     capsAuxCam->video_sizes_tbl[j].width) &&
                    (capsMainCam->video_sizes_tbl[i].height ==
                     capsAuxCam->video_sizes_tbl[j].height)) {
                    if (videoSizesTblCntFinal != i) {
                        capsConsolidated.video_sizes_tbl[videoSizesTblCntFinal].width =
                           capsAuxCam->video_sizes_tbl[j].width;
                        capsConsolidated.video_sizes_tbl[videoSizesTblCntFinal].height =
                           capsMainCam->video_sizes_tbl[j].height;
                    }
                    ++videoSizesTblCntFinal;
                    break;
                }
            }
        }
        capsConsolidated.video_sizes_tbl_cnt = videoSizesTblCntFinal;

        // Consolidate livesnapshot sizes
        uint32_t livesnapshotSizesTblCntMain  = capsMainCam->livesnapshot_sizes_tbl_cnt;
        uint32_t livesnapshotSizesTblCntAux   = capsAuxCam->livesnapshot_sizes_tbl_cnt;
        uint32_t livesnapshotSizesTblCntFinal = 0;

        for (uint32_t i = 0; i < livesnapshotSizesTblCntMain; ++i) {
            for (uint32_t j = 0; j < livesnapshotSizesTblCntAux; ++j) {
                if ((capsMainCam->livesnapshot_sizes_tbl[i].width ==
                     capsAuxCam->livesnapshot_sizes_tbl[j].width) &&
                    (capsMainCam->livesnapshot_sizes_tbl[i].height ==
                     capsAuxCam->livesnapshot_sizes_tbl[j].height)) {
                    if (livesnapshotSizesTblCntFinal != i) {
                       capsConsolidated.livesnapshot_sizes_tbl[livesnapshotSizesTblCntFinal].width=
                          capsAuxCam->livesnapshot_sizes_tbl[j].width;
                       capsConsolidated.livesnapshot_sizes_tbl[livesnapshotSizesTblCntFinal].height=
                          capsMainCam->livesnapshot_sizes_tbl[j].height;
                    }
                    ++livesnapshotSizesTblCntFinal;
                    break;
                }
            }
        }
        capsConsolidated.livesnapshot_sizes_tbl_cnt = livesnapshotSizesTblCntFinal;

        // Consolidate picture size
        // Find max picture dimension for main camera
        cam_dimension_t maxPicDimMain;
        maxPicDimMain.width  = 0;
        maxPicDimMain.height = 0;

        for(uint32_t i = 0; i < (capsMainCam->picture_sizes_tbl_cnt - 1); ++i) {
            if ((maxPicDimMain.width * maxPicDimMain.height) <
                    (capsMainCam->picture_sizes_tbl[i].width *
                            capsMainCam->picture_sizes_tbl[i].height)) {
                maxPicDimMain.width  = capsMainCam->picture_sizes_tbl[i].width;
                maxPicDimMain.height = capsMainCam->picture_sizes_tbl[i].height;
            }
        }

        // Find max picture dimension for aux camera
        cam_dimension_t maxPicDimAux;
        maxPicDimAux.width  = 0;
        maxPicDimAux.height = 0;

        for(uint32_t i = 0; i < (capsAuxCam->picture_sizes_tbl_cnt - 1); ++i) {
            if ((maxPicDimAux.width * maxPicDimAux.height) <
                    (capsAuxCam->picture_sizes_tbl[i].width *
                            capsAuxCam->picture_sizes_tbl[i].height)) {
                maxPicDimAux.width  = capsAuxCam->picture_sizes_tbl[i].width;
                maxPicDimAux.height = capsAuxCam->picture_sizes_tbl[i].height;
            }
        }

        // Choose the smaller of the two max picture dimensions
        if ((maxPicDimAux.width * maxPicDimAux.height) <
                (maxPicDimMain.width * maxPicDimMain.height)) {
            capsConsolidated.picture_sizes_tbl_cnt = capsAuxCam->picture_sizes_tbl_cnt;
            memcpy(capsConsolidated.picture_sizes_tbl, capsAuxCam->picture_sizes_tbl,
                    (capsAuxCam->picture_sizes_tbl_cnt * sizeof(cam_dimension_t)));
        }

        // Consolidate supported preview formats
        uint32_t supportedPreviewFmtCntMain  = capsMainCam->supported_preview_fmt_cnt;
        uint32_t supportedPreviewFmtCntAux   = capsAuxCam->supported_preview_fmt_cnt;
        uint32_t supportedPreviewFmtCntFinal = 0;
        for (uint32_t i = 0; i < supportedPreviewFmtCntMain; ++i) {
            for (uint32_t j = 0; j < supportedPreviewFmtCntAux; ++j) {
                if (capsMainCam->supported_preview_fmts[i] ==
                        capsAuxCam->supported_preview_fmts[j]) {
                    if (supportedPreviewFmtCntFinal != i) {
                        capsConsolidated.supported_preview_fmts[supportedPreviewFmtCntFinal] =
                            capsAuxCam->supported_preview_fmts[j];
                    }
                    ++supportedPreviewFmtCntFinal;
                    break;
                }
            }
        }
        capsConsolidated.supported_preview_fmt_cnt = supportedPreviewFmtCntFinal;

        // Consolidate supported picture formats
        uint32_t supportedPictureFmtCntMain  = capsMainCam->supported_picture_fmt_cnt;
        uint32_t supportedPictureFmtCntAux   = capsAuxCam->supported_picture_fmt_cnt;
        uint32_t supportedPictureFmtCntFinal = 0;
        for (uint32_t i = 0; i < supportedPictureFmtCntMain; ++i) {
            for (uint32_t j = 0; j < supportedPictureFmtCntAux; ++j) {
                if (capsMainCam->supported_picture_fmts[i] ==
                        capsAuxCam->supported_picture_fmts[j]) {
                    if (supportedPictureFmtCntFinal != i) {
                        capsConsolidated.supported_picture_fmts[supportedPictureFmtCntFinal] =
                            capsAuxCam->supported_picture_fmts[j];
                    }
                    ++supportedPictureFmtCntFinal;
                    break;
                }
            }
        }
        capsConsolidated.supported_picture_fmt_cnt = supportedPictureFmtCntFinal;
    }
    return capsConsolidated;
}


/*===========================================================================
 * FUNCTION    : updateConfigSettings
 *
 * DESCRIPTION : Update the config settings such as margins and preview size
 *               and recalculate the transition parameters.
 *
 * PARAMETERS  :
 * @capsMainCam: Capabilities for the main camera
 * @capsAuxCam : Capabilities for the aux camera
 *
 * RETURN :
 * NO_ERROR           : Success
 * INVALID_OPERATION  : Failure
 *
 *==========================================================================*/
int32_t QCameraFOVControl::updateConfigSettings(
        parm_buffer_t* paramsMainCam,
        parm_buffer_t* paramsAuxCam)
{
    int32_t rc = INVALID_OPERATION;

    if (paramsMainCam &&
        paramsAuxCam  &&
        paramsMainCam->is_valid[CAM_INTF_META_STREAM_INFO] &&
        paramsAuxCam->is_valid[CAM_INTF_META_STREAM_INFO]) {

        cam_stream_size_info_t camMainStreamInfo;
        READ_PARAM_ENTRY(paramsMainCam, CAM_INTF_META_STREAM_INFO, camMainStreamInfo);
        mFovControlData.camcorderMode = false;

        // Identify if in camera or camcorder mode
        for (int i = 0; i < MAX_NUM_STREAMS; ++i) {
            if (camMainStreamInfo.type[i] == CAM_STREAM_TYPE_VIDEO) {
                mFovControlData.camcorderMode = true;
            }
        }

        // Get the margins for the main camera. If video stream is present, the margins correspond
        // to video stream. Otherwise, margins are copied from preview stream.
        for (int i = 0; i < MAX_NUM_STREAMS; ++i) {
            if (camMainStreamInfo.type[i] == CAM_STREAM_TYPE_VIDEO) {
                mFovControlData.camMainWidthMargin  = camMainStreamInfo.margins[i].widthMargins;
                mFovControlData.camMainHeightMargin = camMainStreamInfo.margins[i].heightMargins;
            }
            if (camMainStreamInfo.type[i] == CAM_STREAM_TYPE_PREVIEW) {
                // Update the preview dimension
                mFovControlData.previewSize = camMainStreamInfo.stream_sizes[i];
                if (!mFovControlData.camcorderMode) {
                    mFovControlData.camMainWidthMargin  =
                            camMainStreamInfo.margins[i].widthMargins;
                    mFovControlData.camMainHeightMargin =
                            camMainStreamInfo.margins[i].heightMargins;
                    break;
                }
            }
        }

        // Get the margins for the aux camera. If video stream is present, the margins correspond
        // to the video stream. Otherwise, margins are copied from preview stream.
        cam_stream_size_info_t camAuxStreamInfo;
        READ_PARAM_ENTRY(paramsAuxCam, CAM_INTF_META_STREAM_INFO, camAuxStreamInfo);
        for (int i = 0; i < MAX_NUM_STREAMS; ++i) {
            if (camAuxStreamInfo.type[i] == CAM_STREAM_TYPE_VIDEO) {
                mFovControlData.camAuxWidthMargin  = camAuxStreamInfo.margins[i].widthMargins;
                mFovControlData.camAuxHeightMargin = camAuxStreamInfo.margins[i].heightMargins;
            }
            if (camAuxStreamInfo.type[i] == CAM_STREAM_TYPE_PREVIEW) {
                // Update the preview dimension
                mFovControlData.previewSize = camAuxStreamInfo.stream_sizes[i];
                if (!mFovControlData.camcorderMode) {
                    mFovControlData.camAuxWidthMargin  = camAuxStreamInfo.margins[i].widthMargins;
                    mFovControlData.camAuxHeightMargin = camAuxStreamInfo.margins[i].heightMargins;
                    break;
                }
            }
        }

        // Copy the FOV-control settings for camera/camcorder from QCameraFOVControlSettings.h
        if (mFovControlData.camcorderMode) {
            mFovControlConfig.snapshotPPConfig.enablePostProcess =
                    FOVC_CAMCORDER_SNAPSHOT_PP_ENABLE;
        } else {
            mFovControlConfig.snapshotPPConfig.enablePostProcess = FOVC_CAM_SNAPSHOT_PP_ENABLE;
            mFovControlConfig.snapshotPPConfig.zoomMin           = FOVC_CAM_SNAPSHOT_PP_ZOOM_MIN;
            mFovControlConfig.snapshotPPConfig.zoomMax           = FOVC_CAM_SNAPSHOT_PP_ZOOM_MAX;
            mFovControlConfig.snapshotPPConfig.luxMin            = FOVC_CAM_SNAPSHOT_PP_LUX_MIN;
        }
        mFovControlConfig.auxSwitchBrightnessMin  = FOVC_AUXCAM_SWITCH_LUX_MIN;
        mFovControlConfig.auxSwitchFocusDistCmMin = FOVC_AUXCAM_SWITCH_FOCUS_DIST_CM_MIN;

        mFovControlData.fallbackEnabled = FOVC_MAIN_CAM_FALLBACK_MECHANISM;

        // Reset variables
        mFovControlData.zoomStableCount       = 0;
        mFovControlData.brightnessStableCount = 0;
        mFovControlData.focusDistStableCount  = 0;
        mFovControlData.zoomDirection         = ZOOM_STABLE;
        mFovControlData.fallbackToWide        = false;

        // TODO : These threshold values should be changed from counters to time based
        // Systems team will provide the correct values as part of tuning
        mFovControlData.zoomStableCountThreshold       = 30;
        mFovControlData.focusDistStableCountThreshold  = 30;
        mFovControlData.brightnessStableCountThreshold = 30;

        mFovControlData.status3A.main.af.status   = AF_INVALID;
        mFovControlData.status3A.aux.af.status    = AF_INVALID;

        mFovControlData.spatialAlignResult.readyStatus = 0;
        mFovControlData.spatialAlignResult.activeCameras = (uint32_t)CAM_TYPE_MAIN;
        mFovControlData.spatialAlignResult.shiftWide.shiftHorz = 0;
        mFovControlData.spatialAlignResult.shiftWide.shiftVert = 0;
        mFovControlData.spatialAlignResult.shiftTele.shiftHorz = 0;
        mFovControlData.spatialAlignResult.shiftTele.shiftVert = 0;

        // WA for now until the QTI solution is in place writing the spatial alignment ready status
        mFovControlData.spatialAlignResult.readyStatus = 1;

        // Recalculate the transition parameters
        if (calculateBasicFovRatio() && combineFovAdjustment()) {

            calculateDualCamTransitionParams();

            // Set initial camera state
            float zoom = findZoomRatio(mFovControlData.zoomWide) /
                    (float)mFovControlData.zoomRatioTable[0];
            if (zoom > mFovControlData.transitionParams.cutOverWideToTele) {
                mFovControlResult.camMasterPreview  = mFovControlData.camTele;
                mFovControlResult.camMaster3A       = mFovControlData.camTele;
                mFovControlResult.activeCameras     = (uint32_t)mFovControlData.camTele;

                // Initialize spatial alignment results to match with this initial camera state
                mFovControlData.spatialAlignResult.camMasterHint  = mFovControlData.camTele;
                mFovControlData.spatialAlignResult.activeCameras  = mFovControlData.camTele;
                LOGD("start camera state: TELE");
            } else {
                mFovControlResult.camMasterPreview  = mFovControlData.camWide;
                mFovControlResult.camMaster3A       = mFovControlData.camWide;
                mFovControlResult.activeCameras     = (uint32_t)mFovControlData.camWide;

                // Initialize spatial alignment results to match with this initial camera state
                mFovControlData.spatialAlignResult.camMasterHint  = mFovControlData.camWide;
                mFovControlData.spatialAlignResult.activeCameras  = mFovControlData.camWide;
                LOGD("start camera state: WIDE");
            }
            mFovControlResult.snapshotPostProcess = false;

            // FOV-control config is complete for the current use case
            mFovControlData.configCompleted = true;
            rc = NO_ERROR;
        }
    }

    return rc;
}


/*===========================================================================
 * FUNCTION   : translateInputParams
 *
 * DESCRIPTION: Translate a subset of input parameters from main camera. As main
 *              and aux cameras have different properties/params, this translation
 *              is needed before the input parameters are sent to the aux camera.
 *
 * PARAMETERS :
 * @paramsMainCam : Input parameters for main camera
 * @paramsAuxCam  : Input parameters for aux camera
 *
 * RETURN :
 * NO_ERROR           : Success
 * INVALID_OPERATION  : Failure
 *
 *==========================================================================*/
int32_t QCameraFOVControl::translateInputParams(
        parm_buffer_t* paramsMainCam,
        parm_buffer_t* paramsAuxCam)
{
    int32_t rc = INVALID_OPERATION;
    if (paramsMainCam && paramsAuxCam) {
        // First copy all the parameters from main to aux and then translate the subset
        memcpy(paramsAuxCam, paramsMainCam, sizeof(parm_buffer_t));

        // Translate zoom
        if (paramsMainCam->is_valid[CAM_INTF_PARM_ZOOM]) {
            uint32_t userZoom = 0;
            READ_PARAM_ENTRY(paramsMainCam, CAM_INTF_PARM_ZOOM, userZoom);
            convertUserZoomToWideAndTele(userZoom);

            // Update zoom values in the param buffers
            uint32_t zoomAux = isMainCamFovWider() ?
                    mFovControlData.zoomTele : mFovControlData.zoomWide;
            ADD_SET_PARAM_ENTRY_TO_BATCH(paramsAuxCam, CAM_INTF_PARM_ZOOM, zoomAux);

            // Write the updated zoom value for the main camera if the main camera FOV
            // is not the wider of the two.
            if (!isMainCamFovWider()) {
                ADD_SET_PARAM_ENTRY_TO_BATCH(paramsMainCam, CAM_INTF_PARM_ZOOM,
                        mFovControlData.zoomTele);
            }

            // Write the user zoom in main and aux param buffers
            // The user zoom will always correspond to the wider camera
            paramsMainCam->is_valid[CAM_INTF_PARM_DC_USERZOOM] = 1;
            paramsAuxCam->is_valid[CAM_INTF_PARM_DC_USERZOOM]  = 1;

            ADD_SET_PARAM_ENTRY_TO_BATCH(paramsMainCam, CAM_INTF_PARM_DC_USERZOOM,
                    mFovControlData.zoomWide);
            ADD_SET_PARAM_ENTRY_TO_BATCH(paramsAuxCam, CAM_INTF_PARM_DC_USERZOOM,
                    mFovControlData.zoomWide);

            // Generate FOV-control result
            generateFovControlResult();
        }

        if (paramsMainCam->is_valid[CAM_INTF_PARM_AF_ROI] ||
            paramsMainCam->is_valid[CAM_INTF_PARM_AEC_ROI]) {
            convertDisparityForInputParams();
        }

        // Translate focus areas
        if (paramsMainCam->is_valid[CAM_INTF_PARM_AF_ROI]) {
            cam_roi_info_t roiAfMain;
            cam_roi_info_t roiAfAux;
            READ_PARAM_ENTRY(paramsMainCam, CAM_INTF_PARM_AF_ROI, roiAfMain);
            if (roiAfMain.num_roi > 0) {
                roiAfAux = translateFocusAreas(roiAfMain);
                ADD_SET_PARAM_ENTRY_TO_BATCH(paramsAuxCam, CAM_INTF_PARM_AF_ROI, roiAfAux);
            }
        }

        // Translate metering areas
        if (paramsMainCam->is_valid[CAM_INTF_PARM_AEC_ROI]) {
            cam_set_aec_roi_t roiAecMain;
            cam_set_aec_roi_t roiAecAux;
            READ_PARAM_ENTRY(paramsMainCam, CAM_INTF_PARM_AEC_ROI, roiAecMain);
            if (roiAecMain.aec_roi_enable == CAM_AEC_ROI_ON) {
                roiAecAux = translateMeteringAreas(roiAecMain);
                ADD_SET_PARAM_ENTRY_TO_BATCH(paramsAuxCam, CAM_INTF_PARM_AEC_ROI, roiAecAux);
            }
        }
        rc = NO_ERROR;
    }
    return rc;
}


/*===========================================================================
 * FUNCTION   : processResultMetadata
 *
 * DESCRIPTION: Process the metadata from main and aux cameras to generate the
 *              result metadata. The result metadata should be the metadata
 *              coming from the master camera. If aux camera is master, the
 *              subset of the metadata needs to be translated to main as that's
 *              the only camera seen by the application.
 *
 * PARAMETERS :
 * @metaMain  : metadata for main camera
 * @metaAux   : metadata for aux camera
 *
 * RETURN :
 * Result metadata for the logical camera. After successfully processing main
 * and aux metadata, the result metadata points to either main or aux metadata
 * based on which one was the master. In case of failure, it returns NULL.
 *==========================================================================*/
metadata_buffer_t* QCameraFOVControl::processResultMetadata(
        metadata_buffer_t*  metaMain,
        metadata_buffer_t*  metaAux)
{
    metadata_buffer_t* metaResult = NULL;

    if (metaMain || metaAux) {
        metadata_buffer_t *meta   = metaMain ? metaMain : metaAux;
        cam_sync_type_t masterCam = mFovControlResult.camMasterPreview;

        mMutex.lock();
        // Book-keep the needed metadata from main camera and aux camera
        IF_META_AVAILABLE(cam_sac_output_info_t, spatialAlignOutput,
                CAM_INTF_META_DC_SAC_OUTPUT_INFO, meta) {

            // Get master camera hint
            if (spatialAlignOutput->is_master_hint_valid) {
                uint8_t master = spatialAlignOutput->master_hint;
                if (master == CAM_ROLE_WIDE) {
                    mFovControlData.spatialAlignResult.camMasterHint = mFovControlData.camWide;
                } else if (master == CAM_ROLE_TELE) {
                    mFovControlData.spatialAlignResult.camMasterHint = mFovControlData.camTele;
                }
            }

            // Get master camera used for the preview in the frame corresponding to this metadata
            if (spatialAlignOutput->is_master_preview_valid) {
                uint8_t master = spatialAlignOutput->master_preview;
                if (master == CAM_ROLE_WIDE) {
                    masterCam = mFovControlData.camWide;
                    mFovControlData.spatialAlignResult.camMasterPreview = masterCam;
                } else if (master == CAM_ROLE_TELE) {
                    masterCam = mFovControlData.camTele;
                    mFovControlData.spatialAlignResult.camMasterPreview = masterCam;
                }
            }

            // Get master camera used for 3A in the frame corresponding to this metadata
            if (spatialAlignOutput->is_master_3A_valid) {
                uint8_t master = spatialAlignOutput->master_3A;
                if (master == CAM_ROLE_WIDE) {
                    mFovControlData.spatialAlignResult.camMaster3A = mFovControlData.camWide;
                } else if (master == CAM_ROLE_TELE) {
                    mFovControlData.spatialAlignResult.camMaster3A = mFovControlData.camTele;
                }
            }

            // Get spatial alignment ready status
            if (spatialAlignOutput->is_ready_status_valid) {
                mFovControlData.spatialAlignResult.readyStatus = spatialAlignOutput->ready_status;
            }
        }

        // Get spatial alignment output shift for main camera
        if (metaMain) {
            IF_META_AVAILABLE(cam_sac_output_info_t, spatialAlignOutput,
                CAM_INTF_META_DC_SAC_OUTPUT_INFO, metaMain) {
                if (spatialAlignOutput->is_output_shift_valid) {
                    // Calculate the spatial alignment shift for the current stream dimensions based
                    // on the reference resolution used for the output shift.
                    float horzShiftFactor = mFovControlData.previewSize.width /
                            spatialAlignOutput->reference_res_for_output_shift.width;
                    float vertShiftFactor = mFovControlData.previewSize.height /
                            spatialAlignOutput->reference_res_for_output_shift.height;

                    if (isMainCamFovWider()) {
                        mFovControlData.spatialAlignResult.shiftWide.shiftHorz =
                                spatialAlignOutput->output_shift.shift_horz * horzShiftFactor;
                        mFovControlData.spatialAlignResult.shiftWide.shiftVert =
                                spatialAlignOutput->output_shift.shift_vert * vertShiftFactor;
                    } else {
                        mFovControlData.spatialAlignResult.shiftTele.shiftHorz =
                                spatialAlignOutput->output_shift.shift_horz * horzShiftFactor;
                        mFovControlData.spatialAlignResult.shiftTele.shiftVert =
                                spatialAlignOutput->output_shift.shift_vert * vertShiftFactor;
                    }
                }
            }
        }

        // Get spatial alignment output shift for aux camera
        if (metaAux) {
            IF_META_AVAILABLE(cam_sac_output_info_t, spatialAlignOutput,
                CAM_INTF_META_DC_SAC_OUTPUT_INFO, metaAux) {
                if (spatialAlignOutput->is_output_shift_valid) {
                    // Calculate the spatial alignment shift for the current stream dimensions based
                    // on the reference resolution used for the output shift.
                    float horzShiftFactor = mFovControlData.previewSize.width /
                            spatialAlignOutput->reference_res_for_output_shift.width;
                    float vertShiftFactor = mFovControlData.previewSize.height /
                            spatialAlignOutput->reference_res_for_output_shift.height;

                    if (isMainCamFovWider()) {
                        mFovControlData.spatialAlignResult.shiftTele.shiftHorz =
                                spatialAlignOutput->output_shift.shift_horz * horzShiftFactor;
                        mFovControlData.spatialAlignResult.shiftTele.shiftVert =
                                spatialAlignOutput->output_shift.shift_vert * vertShiftFactor;
                    } else {
                        mFovControlData.spatialAlignResult.shiftWide.shiftHorz =
                                spatialAlignOutput->output_shift.shift_horz * horzShiftFactor;
                        mFovControlData.spatialAlignResult.shiftWide.shiftVert =
                                spatialAlignOutput->output_shift.shift_vert * vertShiftFactor;
                    }
                }
            }
        }

        if (mFovControlData.availableSpatialAlignSolns & CAM_SPATIAL_ALIGN_OEM) {
            // Get low power mode info
            metadata_buffer_t *metaInactiveCam = (masterCam == CAM_TYPE_MAIN) ? metaAux : metaMain;
            if (metaInactiveCam) {
                IF_META_AVAILABLE(uint8_t, enableLPM, CAM_INTF_META_DC_LOW_POWER_ENABLE,
                        metaInactiveCam) {
                    if (*enableLPM) {
                        mFovControlData.spatialAlignResult.activeCameras = masterCam;
                    }
                }
            }
        }

        // Get AF status
        uint32_t afStatusMain = CAM_AF_STATE_INACTIVE;
        uint32_t afStatusAux  = CAM_AF_STATE_INACTIVE;
        if (metaMain) {
            IF_META_AVAILABLE(uint32_t, afState, CAM_INTF_META_AF_STATE, metaMain) {
                if (((*afState) == CAM_AF_STATE_FOCUSED_LOCKED)         ||
                        ((*afState) == CAM_AF_STATE_NOT_FOCUSED_LOCKED) ||
                        ((*afState) == CAM_AF_STATE_PASSIVE_FOCUSED)    ||
                        ((*afState) == CAM_AF_STATE_PASSIVE_UNFOCUSED)) {
                    mFovControlData.status3A.main.af.status = AF_VALID;
                } else {
                    mFovControlData.status3A.main.af.status = AF_INVALID;
                }
                afStatusMain = *afState;
                LOGD("AF state: Main cam: %d", afStatusMain);
            }
            // WA:Hardcoding dummy lux and focusDistance metadata till that functionality gets added
            mFovControlData.status3A.main.ae.lux         = 1000;
            mFovControlData.status3A.main.af.focusDistCm = 100;
        }
        if (metaAux) {
            IF_META_AVAILABLE(uint32_t, afState, CAM_INTF_META_AF_STATE, metaAux) {
                if (((*afState) == CAM_AF_STATE_FOCUSED_LOCKED)         ||
                        ((*afState) == CAM_AF_STATE_NOT_FOCUSED_LOCKED) ||
                        ((*afState) == CAM_AF_STATE_PASSIVE_FOCUSED)    ||
                        ((*afState) == CAM_AF_STATE_PASSIVE_UNFOCUSED)) {
                    mFovControlData.status3A.aux.af.status = AF_VALID;
                } else {
                    mFovControlData.status3A.aux.af.status = AF_INVALID;
                }
                afStatusAux = *afState;
                LOGD("AF state: Aux cam: %d", afStatusAux);
            }
        }

        metadata_buffer_t *metaWide;
        metadata_buffer_t *metaTele;

        // Check if the wide, tele cameras are streaming
        if (isMainCamFovWider()) {
            metaWide = metaMain;
            metaTele = metaAux;
        } else {
            metaWide = metaAux;
            metaTele = metaMain;
        }

        if (metaWide) {
            mFovControlData.wideCamStreaming = true;
        } else {
            mFovControlData.wideCamStreaming = false;
        }
        if (metaTele) {
            mFovControlData.teleCamStreaming = true;
        } else {
            mFovControlData.teleCamStreaming = false;
        }

        mMutex.unlock();

        if ((masterCam == CAM_TYPE_AUX) && metaAux) {
            // Translate face detection ROI
            IF_META_AVAILABLE(cam_face_detection_data_t, metaFD,
                    CAM_INTF_META_FACE_DETECTION, metaAux) {
                cam_face_detection_data_t metaFDTranslated;
                metaFDTranslated = translateRoiFD(*metaFD);
                ADD_SET_PARAM_ENTRY_TO_BATCH(metaAux, CAM_INTF_META_FACE_DETECTION,
                        metaFDTranslated);
            }
            metaResult = metaAux;
        }
        else if ((masterCam == CAM_TYPE_MAIN) && metaMain) {
            metaResult = metaMain;
        } else {
            // Metadata for the master camera was dropped
            metaResult = NULL;
        }

        // Consolidate the AF status to be sent to the app
        // Only return focused if both are focused.
        if (metaMain && metaAux) {
            if (((afStatusMain == CAM_AF_STATE_FOCUSED_LOCKED) ||
                    (afStatusMain == CAM_AF_STATE_NOT_FOCUSED_LOCKED)) &&
                    ((afStatusAux == CAM_AF_STATE_FOCUSED_LOCKED) ||
                    (afStatusAux == CAM_AF_STATE_NOT_FOCUSED_LOCKED))) {
                // If both indicate focused, return focused.
                // If either one indicates 'not focused', return 'not focused'.
                if ((afStatusMain == CAM_AF_STATE_FOCUSED_LOCKED) &&
                        (afStatusAux  == CAM_AF_STATE_FOCUSED_LOCKED)) {
                    ADD_SET_PARAM_ENTRY_TO_BATCH(metaResult, CAM_INTF_META_AF_STATE,
                            CAM_AF_STATE_FOCUSED_LOCKED);
                } else {
                    ADD_SET_PARAM_ENTRY_TO_BATCH(metaResult, CAM_INTF_META_AF_STATE,
                            CAM_AF_STATE_NOT_FOCUSED_LOCKED);
                }
            } else {
                // If either one indicates passive state or active scan, return that state
                if ((afStatusMain != CAM_AF_STATE_FOCUSED_LOCKED) &&
                        (afStatusMain != CAM_AF_STATE_NOT_FOCUSED_LOCKED)) {
                    ADD_SET_PARAM_ENTRY_TO_BATCH(metaResult, CAM_INTF_META_AF_STATE, afStatusMain);
                } else {
                    ADD_SET_PARAM_ENTRY_TO_BATCH(metaResult, CAM_INTF_META_AF_STATE, afStatusAux);
                }
            }
            IF_META_AVAILABLE(uint32_t, afState, CAM_INTF_META_AF_STATE, metaResult) {
                LOGD("Result AF state: %d", *afState);
            }
        }

        // Generate FOV-control result
        generateFovControlResult();
    }
    return metaResult;
}


/*===========================================================================
 * FUNCTION   : generateFovControlResult
 *
 * DESCRIPTION: Generate FOV control result
 *
 * PARAMETERS : None
 *
 * RETURN     : None
 *
 *==========================================================================*/
void QCameraFOVControl::generateFovControlResult()
{
    Mutex::Autolock lock(mMutex);

    float zoom = findZoomRatio(mFovControlData.zoomWide) / (float)mFovControlData.zoomRatioTable[0];
    uint32_t zoomWide     = mFovControlData.zoomWide;
    uint32_t zoomWidePrev = mFovControlData.zoomWidePrev;

    if (mFovControlData.configCompleted == false) {
        // Return as invalid result if the FOV-control configuration is not yet complete
        mFovControlResult.isValid = false;
        return;
    }

    // Update previous zoom value
    mFovControlData.zoomWidePrev = mFovControlData.zoomWide;

    uint16_t  currentBrightness = mFovControlData.status3A.main.ae.lux;
    uint16_t  currentFocusDist  = mFovControlData.status3A.main.af.focusDistCm;

    af_status afStatusAux = mFovControlData.status3A.aux.af.status;

    float transitionLow     = mFovControlData.transitionParams.transitionLow;
    float transitionHigh    = mFovControlData.transitionParams.transitionHigh;
    float cutOverWideToTele = mFovControlData.transitionParams.cutOverWideToTele;
    float cutOverTeleToWide = mFovControlData.transitionParams.cutOverTeleToWide;

    cam_sync_type_t camWide = mFovControlData.camWide;
    cam_sync_type_t camTele = mFovControlData.camTele;

    uint16_t thresholdBrightness = mFovControlConfig.auxSwitchBrightnessMin;
    uint16_t thresholdFocusDist  = mFovControlConfig.auxSwitchFocusDistCmMin;

    if (zoomWide == zoomWidePrev) {
        mFovControlData.zoomDirection = ZOOM_STABLE;
        ++mFovControlData.zoomStableCount;
    } else if (zoomWide > zoomWidePrev) {
        mFovControlData.zoomDirection   = ZOOM_IN;
        mFovControlData.zoomStableCount = 0;
    } else {
        mFovControlData.zoomDirection   = ZOOM_OUT;
        mFovControlData.zoomStableCount = 0;
    }

    // Update snapshot post-process flags
    if (mFovControlConfig.snapshotPPConfig.enablePostProcess &&
        (zoom >= mFovControlConfig.snapshotPPConfig.zoomMin) &&
        (zoom <= mFovControlConfig.snapshotPPConfig.zoomMax)) {
        mFovControlResult.snapshotPostProcessZoomRange = true;
    } else {
        mFovControlResult.snapshotPostProcessZoomRange = false;
    }

    if (mFovControlResult.snapshotPostProcessZoomRange &&
        (currentBrightness >= mFovControlConfig.snapshotPPConfig.luxMin) &&
        (currentFocusDist  >= mFovControlConfig.snapshotPPConfig.focusDistanceMin)) {
        mFovControlResult.snapshotPostProcess = true;
    } else {
        mFovControlResult.snapshotPostProcess = false;
    }

    switch (mFovControlData.camState) {
        case STATE_WIDE:
            // If the scene continues to be bright, update stable count; reset otherwise
            if (currentBrightness >= thresholdBrightness) {
                ++mFovControlData.brightnessStableCount;
            } else {
                mFovControlData.brightnessStableCount = 0;
            }

            // If the scene continues to be non-macro, update stable count; reset otherwise
            if (currentFocusDist >= thresholdFocusDist) {
                ++mFovControlData.focusDistStableCount;
            } else {
                mFovControlData.focusDistStableCount = 0;
            }

            // Reset fallback to main flag if zoom is less than cutover point
            if (zoom <= cutOverTeleToWide) {
                mFovControlData.fallbackToWide = false;
            }

            // Check if the scene is good for aux (bright and far focused)
            if ((currentBrightness >= thresholdBrightness) &&
                (currentFocusDist >= thresholdFocusDist)) {
                // Lower constraint if zooming in or if snapshot postprocessing is true
                if (mFovControlResult.snapshotPostProcess ||
                    (((zoom >= transitionLow) ||
                     (mFovControlData.spatialAlignResult.activeCameras == (camWide | camTele))) &&
                    (mFovControlData.zoomDirection == ZOOM_IN) &&
                    (mFovControlData.fallbackToWide == false))) {
                    mFovControlData.camState = STATE_TRANSITION;
                    mFovControlResult.activeCameras = (camWide | camTele);
                }
                // Higher constraint if not zooming in
                else if ((zoom > cutOverWideToTele) &&
                    (mFovControlData.brightnessStableCount >=
                            mFovControlData.brightnessStableCountThreshold) &&
                    (mFovControlData.focusDistStableCount  >=
                            mFovControlData.focusDistStableCountThreshold)) {
                    // Enter the transition state
                    mFovControlData.camState = STATE_TRANSITION;
                    mFovControlResult.activeCameras = (camWide | camTele);

                    // Reset fallback to wide flag
                    mFovControlData.fallbackToWide = false;
                }
            }
            break;

        case STATE_TRANSITION:
            // Reset brightness stable count
            mFovControlData.brightnessStableCount = 0;
            // Reset focus distance stable count
            mFovControlData.focusDistStableCount  = 0;

            // Set the master info
            // Switch to wide
            if ((mFovControlData.fallbackEnabled && mFovControlData.fallbackToWide) ||
                ((zoom < cutOverTeleToWide))) {
                // If wide cam is aux, check AF and spatial alignment data validity for switch
                if (camWide == CAM_TYPE_AUX) {
                    if ((afStatusAux == AF_VALID) &&
                        isSpatialAlignmentReady()) {
                        mFovControlResult.camMasterPreview = camWide;
                        mFovControlResult.camMaster3A      = camWide;
                    }
                }
                // If wide cam is not aux, switch as long as the wide cam is streaming
                else if (mFovControlData.wideCamStreaming) {
                    mFovControlResult.camMasterPreview = camWide;
                    mFovControlResult.camMaster3A      = camWide;
                }
            }
            // switch to tele
            else if (zoom > cutOverWideToTele) {
                // If tele cam is aux, check AF and spatial alignment data validity for switch
                if (camTele == CAM_TYPE_AUX) {
                    if ((afStatusAux == AF_VALID) &&
                        isSpatialAlignmentReady()) {
                        mFovControlResult.camMasterPreview = camTele;
                        mFovControlResult.camMaster3A      = camTele;
                    }
                }
                // If tele cam is not aux, switch as long as the tele cam is streaming
                else if (mFovControlData.teleCamStreaming) {
                    mFovControlResult.camMasterPreview = camTele;
                    mFovControlResult.camMaster3A      = camTele;
                }
            }

            // Change the transition state if necessary.
            // If snapshot post processing is required, do not change the state.
            // TODO : If zoom is stable put the inactive camera to LPM
            if (mFovControlResult.snapshotPostProcess == false) {
                if ((zoom < transitionLow) &&
                    (mFovControlData.spatialAlignResult.activeCameras != (camWide | camTele))) {
                    mFovControlData.camState        = STATE_WIDE;
                    mFovControlResult.activeCameras = camWide;
                } else if ((zoom > transitionHigh) &&
                    (mFovControlData.spatialAlignResult.activeCameras != (camWide | camTele))) {
                    mFovControlData.camState        = STATE_TELE;
                    mFovControlResult.activeCameras = camTele;
                }
            }
            break;

        case STATE_TELE:
            // If the scene continues to be dark, update stable count; reset otherwise
            if (currentBrightness < thresholdBrightness) {
                ++mFovControlData.brightnessStableCount;
            } else {
                mFovControlData.brightnessStableCount = 0;
            }

            // If the scene continues to be macro, update stable count; reset otherwise
            if (currentFocusDist < thresholdFocusDist) {
                ++mFovControlData.focusDistStableCount;
            } else {
                mFovControlData.focusDistStableCount = 0;
            }

            // Lower constraint if zooming out or if the snapshot postprocessing is true
            if (mFovControlResult.snapshotPostProcess ||
                (((zoom <= transitionHigh) ||
                 (mFovControlData.spatialAlignResult.activeCameras == (camWide | camTele))) &&
                (mFovControlData.zoomDirection == ZOOM_OUT))) {
                mFovControlData.camState = STATE_TRANSITION;
                mFovControlResult.activeCameras = (camWide | camTele);
            }
            // Higher constraint if not zooming out
            else if ((currentBrightness < thresholdBrightness) ||
                (currentFocusDist < thresholdFocusDist)) {
                // Enter transition state if brightness or focus distance is below threshold
                if ((mFovControlData.brightnessStableCount >=
                            mFovControlData.brightnessStableCountThreshold) ||
                    (mFovControlData.focusDistStableCount  >=
                            mFovControlData.focusDistStableCountThreshold)) {
                    mFovControlData.camState = STATE_TRANSITION;
                    mFovControlResult.activeCameras = (camWide | camTele);

                    // Set flag indicating fallback to wide
                    if (mFovControlData.fallbackEnabled) {
                        mFovControlData.fallbackToWide = true;
                    }
                }
            }
            break;
    }

    mFovControlResult.isValid = true;
    // Debug print for the FOV-control result
    LOGD("Effective zoom: %f", zoom);
    LOGD("zoom direction: %d", (uint32_t)mFovControlData.zoomDirection);
    LOGD("zoomWide: %d, zoomTele: %d", zoomWide, mFovControlData.zoomTele);
    LOGD("Snapshot postprocess: %d", mFovControlResult.snapshotPostProcess);
    LOGD("Master camera            : %s", (mFovControlResult.camMasterPreview == CAM_TYPE_MAIN) ?
            "CAM_TYPE_MAIN" : "CAM_TYPE_AUX");
    LOGD("Master camera for preview: %s",
            (mFovControlResult.camMasterPreview == camWide ) ? "Wide" : "Tele");
    LOGD("Master camera for 3A     : %s",
            (mFovControlResult.camMaster3A == camWide ) ? "Wide" : "Tele");
    LOGD("Wide camera status : %s",
            (mFovControlResult.activeCameras & camWide) ? "Active" : "LPM");
    LOGD("Tele camera status : %s",
            (mFovControlResult.activeCameras & camTele) ? "Active" : "LPM");
    LOGD("transition state: %s", ((mFovControlData.camState == STATE_WIDE) ? "STATE_WIDE" :
            ((mFovControlData.camState == STATE_TELE) ? "STATE_TELE" : "STATE_TRANSITION" )));
}


/*===========================================================================
 * FUNCTION   : getFovControlResult
 *
 * DESCRIPTION: Return FOV-control result
 *
 * PARAMETERS : None
 *
 * RETURN     : FOV-control result
 *
 *==========================================================================*/
 fov_control_result_t QCameraFOVControl::getFovControlResult()
{
    Mutex::Autolock lock(mMutex);
    fov_control_result_t fovControlResult = mFovControlResult;
    return fovControlResult;
}


/*===========================================================================
 * FUNCTION    : isMainCamFovWider
 *
 * DESCRIPTION : Check if the main camera FOV is wider than aux
 *
 * PARAMETERS  : None
 *
 * RETURN      :
 * true        : If main cam FOV is wider than tele
 * false       : If main cam FOV is narrower than tele
 *
 *==========================================================================*/
inline bool QCameraFOVControl::isMainCamFovWider()
{
    if (mDualCamParams.paramsMain.focalLengthMm <
            mDualCamParams.paramsAux.focalLengthMm) {
        return true;
    } else {
        return false;
    }
}


/*===========================================================================
 * FUNCTION    : isSpatialAlignmentReady
 *
 * DESCRIPTION : Check if the spatial alignment is ready.
 *               For QTI solution, check ready_status flag
 *               For OEM solution, check camMasterHint
 *               If the spatial alignment solution is not needed, return true
 *
 * PARAMETERS  : None
 *
 * RETURN      :
 * true        : If spatial alignment is ready
 * false       : If spatial alignment is not yet ready
 *
 *==========================================================================*/
bool QCameraFOVControl::isSpatialAlignmentReady()
{
    bool ret = true;

    if (mFovControlData.availableSpatialAlignSolns & CAM_SPATIAL_ALIGN_OEM) {
        uint8_t currentMaster = (uint8_t)mFovControlResult.camMasterPreview;
        uint8_t camMasterHint = mFovControlData.spatialAlignResult.camMasterHint;

        if (currentMaster != camMasterHint) {
            ret = true;
        } else {
            ret = false;
        }
    } else if (mFovControlData.availableSpatialAlignSolns & CAM_SPATIAL_ALIGN_QTI) {
        if (mFovControlData.spatialAlignResult.readyStatus) {
            ret = true;
        } else {
            ret = false;
        }
    }

    return ret;
}


/*===========================================================================
 * FUNCTION    : validateAndExtractParameters
 *
 * DESCRIPTION : Validates a subset of parameters from capabilities and
 *               saves those parameters for decision making.
 *
 * PARAMETERS  :
 *  @capsMain  : The capabilities for the main camera
 *  @capsAux   : The capabilities for the aux camera
 *
 * RETURN      :
 * true        : Success
 * false       : Failure
 *
 *==========================================================================*/
bool QCameraFOVControl::validateAndExtractParameters(
        cam_capability_t  *capsMainCam,
        cam_capability_t  *capsAuxCam)
{
    bool rc = false;
    if (capsMainCam && capsAuxCam) {

        mFovControlConfig.percentMarginHysterisis  = 5;
        mFovControlConfig.percentMarginMain        = 10;
        mFovControlConfig.percentMarginAux         = 15;
        mFovControlConfig.waitTimeForHandoffMs     = 1000;

        // Temporary workaround to avoid wrong calculations with B+B/B+M modules with similar FOV
        // Once W+T modules are available, hardcoded path will be removed
        if ((uint32_t)(capsMainCam->focal_length * 100) !=
                (uint32_t)(capsAuxCam->focal_length * 100)) {
            mDualCamParams.paramsMain.sensorStreamWidth =
                    capsMainCam->related_cam_calibration.main_cam_specific_calibration.\
                    native_sensor_resolution_width;
            mDualCamParams.paramsMain.sensorStreamHeight =
                    capsMainCam->related_cam_calibration.main_cam_specific_calibration.\
                    native_sensor_resolution_height;

            mDualCamParams.paramsAux.sensorStreamWidth   =
                    capsMainCam->related_cam_calibration.aux_cam_specific_calibration.\
                    native_sensor_resolution_width;
            mDualCamParams.paramsAux.sensorStreamHeight  =
                    capsMainCam->related_cam_calibration.aux_cam_specific_calibration.\
                    native_sensor_resolution_height;

            mDualCamParams.paramsMain.focalLengthMm = capsMainCam->focal_length;
            mDualCamParams.paramsAux.focalLengthMm  = capsAuxCam->focal_length;

            mDualCamParams.paramsMain.pixelPitchUm = capsMainCam->pixel_pitch_um;
            mDualCamParams.paramsAux.pixelPitchUm  = capsAuxCam->pixel_pitch_um;

            mDualCamParams.baselineMm   =
                    capsMainCam->related_cam_calibration.relative_baseline_distance;
            mDualCamParams.rollDegrees  = capsMainCam->max_roll_degrees;
            mDualCamParams.pitchDegrees = capsMainCam->max_pitch_degrees;
            mDualCamParams.yawDegrees   = capsMainCam->max_yaw_degrees;

            if ((capsMainCam->min_focus_distance > 0) &&
                    (capsAuxCam->min_focus_distance > 0)) {
                // Convert the min focus distance from diopters to cm
                // and choose the max of both sensors.
                uint32_t minFocusDistCmMain = (100.0f / capsMainCam->min_focus_distance);
                uint32_t minFocusDistCmAux  = (100.0f / capsAuxCam->min_focus_distance);
                mDualCamParams.minFocusDistanceCm = (minFocusDistCmMain > minFocusDistCmAux) ?
                        minFocusDistCmMain : minFocusDistCmAux;
            }

            if (capsMainCam->related_cam_calibration.relative_position_flag == 0) {
                mDualCamParams.positionAux = CAM_POSITION_RIGHT;
            } else {
                mDualCamParams.positionAux = CAM_POSITION_LEFT;
            }
        } else {
            // Hardcoded values till W + T module is available
            mDualCamParams.paramsMain.sensorStreamWidth  = 4208;
            mDualCamParams.paramsMain.sensorStreamHeight = 3120;
            mDualCamParams.paramsMain.pixelPitchUm       = 1.12;
            mDualCamParams.paramsMain.focalLengthMm      = 3.5;
            mDualCamParams.paramsAux.sensorStreamWidth   = 4208;
            mDualCamParams.paramsAux.sensorStreamHeight  = 3120;
            mDualCamParams.paramsAux.pixelPitchUm        = 1.12;
            mDualCamParams.paramsAux.focalLengthMm       = 7;
            mDualCamParams.baselineMm                    = 9.5;
            mDualCamParams.minFocusDistanceCm            = 30;
            mDualCamParams.rollDegrees                   = 1.0;
            mDualCamParams.pitchDegrees                  = 1.0;
            mDualCamParams.yawDegrees                    = 1.0;
            mDualCamParams.positionAux                   = CAM_POSITION_LEFT;
        }

        if ((capsMainCam->avail_spatial_align_solns & CAM_SPATIAL_ALIGN_QTI) ||
                (capsMainCam->avail_spatial_align_solns & CAM_SPATIAL_ALIGN_OEM)) {
            mFovControlData.availableSpatialAlignSolns =
                    capsMainCam->avail_spatial_align_solns;
        } else {
            LOGW("Spatial alignment not supported");
        }

        if (capsMainCam->zoom_supported > 0) {
            mFovControlData.zoomRatioTable      = capsMainCam->zoom_ratio_tbl;
            mFovControlData.zoomRatioTableCount = capsMainCam->zoom_ratio_tbl_cnt;
        } else {
            LOGE("zoom feature not supported");
            return false;
        }
        rc = true;
    }

    return rc;
}

/*===========================================================================
 * FUNCTION   : calculateBasicFovRatio
 *
 * DESCRIPTION: Calculate the FOV ratio between main and aux cameras
 *
 * PARAMETERS : None
 *
 * RETURN     :
 * true       : Success
 * false      : Failure
 *
 *==========================================================================*/
bool QCameraFOVControl::calculateBasicFovRatio()
{
    float fovWide = 0.0f;
    float fovTele = 0.0f;
    bool rc = false;

    if ((mDualCamParams.paramsMain.focalLengthMm > 0.0f) &&
         (mDualCamParams.paramsAux.focalLengthMm > 0.0f)) {
        if (mDualCamParams.paramsMain.focalLengthMm <
            mDualCamParams.paramsAux.focalLengthMm) {
            fovWide = (mDualCamParams.paramsMain.sensorStreamWidth *
                        mDualCamParams.paramsMain.pixelPitchUm) /
                        mDualCamParams.paramsMain.focalLengthMm;

            fovTele = (mDualCamParams.paramsAux.sensorStreamWidth *
                        mDualCamParams.paramsAux.pixelPitchUm) /
                        mDualCamParams.paramsAux.focalLengthMm;
        } else {
            fovWide = (mDualCamParams.paramsAux.sensorStreamWidth *
                        mDualCamParams.paramsAux.pixelPitchUm) /
                        mDualCamParams.paramsAux.focalLengthMm;

            fovTele = (mDualCamParams.paramsMain.sensorStreamWidth *
                        mDualCamParams.paramsMain.pixelPitchUm) /
                        mDualCamParams.paramsMain.focalLengthMm;
        }
        if (fovTele > 0.0f) {
            mFovControlData.basicFovRatio = (fovWide / fovTele);
            rc = true;
        }
    }

    LOGD("Main cam focalLengthMm : %f", mDualCamParams.paramsMain.focalLengthMm);
    LOGD("Aux  cam focalLengthMm : %f", mDualCamParams.paramsAux.focalLengthMm);
    LOGD("Main cam sensorStreamWidth : %u", mDualCamParams.paramsMain.sensorStreamWidth);
    LOGD("Main cam pixelPitchUm      : %f", mDualCamParams.paramsMain.pixelPitchUm);
    LOGD("Main cam focalLengthMm     : %f", mDualCamParams.paramsMain.focalLengthMm);
    LOGD("Aux cam sensorStreamWidth  : %u", mDualCamParams.paramsAux.sensorStreamWidth);
    LOGD("Aux cam pixelPitchUm       : %f", mDualCamParams.paramsAux.pixelPitchUm);
    LOGD("Aux cam focalLengthMm      : %f", mDualCamParams.paramsAux.focalLengthMm);
    LOGD("fov wide : %f", fovWide);
    LOGD("fov tele : %f", fovTele);
    LOGD("BasicFovRatio : %f", mFovControlData.basicFovRatio);

    return rc;
}


/*===========================================================================
 * FUNCTION   : combineFovAdjustment
 *
 * DESCRIPTION: Calculate the final FOV adjustment by combining basic FOV ratio
 *              with the margin info
 *
 * PARAMETERS : None
 *
 * RETURN     :
 * true       : Success
 * false      : Failure
 *
 *==========================================================================*/
bool QCameraFOVControl::combineFovAdjustment()
{
    float ratioMarginWidth;
    float ratioMarginHeight;
    float adjustedRatio;
    bool rc = false;

    ratioMarginWidth = (1.0 + (mFovControlData.camMainWidthMargin)) /
            (1.0 + (mFovControlData.camAuxWidthMargin));
    ratioMarginHeight = (1.0 + (mFovControlData.camMainHeightMargin)) /
            (1.0 + (mFovControlData.camAuxHeightMargin));

    adjustedRatio = (ratioMarginHeight < ratioMarginWidth) ? ratioMarginHeight : ratioMarginWidth;

    if (adjustedRatio > 0.0f) {
        mFovControlData.transitionParams.cutOverFactor =
                (mFovControlData.basicFovRatio / adjustedRatio);
        rc = true;
    }

    LOGD("Main cam margin for width  : %f", mFovControlData.camMainWidthMargin);
    LOGD("Main cam margin for height : %f", mFovControlData.camMainHeightMargin);
    LOGD("Aux  cam margin for width  : %f", mFovControlData.camAuxWidthMargin);
    LOGD("Aux  cam margin for height : %f", mFovControlData.camAuxHeightMargin);
    LOGD("Width  margin ratio : %f", ratioMarginWidth);
    LOGD("Height margin ratio : %f", ratioMarginHeight);

    return rc;
}


/*===========================================================================
 * FUNCTION   : calculateDualCamTransitionParams
 *
 * DESCRIPTION: Calculate the transition parameters needed to switch the camera
 *              between main and aux
 *
 * PARAMETERS :
 * @fovAdjustBasic       : basic FOV ratio
 * @zoomTranslationFactor: translation factor for main, aux zoom
 *
 * RETURN     : none
 *
 *==========================================================================*/
void QCameraFOVControl::calculateDualCamTransitionParams()
{
    float percentMarginWide;
    float percentMarginTele;

    if (isMainCamFovWider()) {
        percentMarginWide = mFovControlConfig.percentMarginMain;
        percentMarginTele = mFovControlConfig.percentMarginAux;
    } else {
        percentMarginWide = mFovControlConfig.percentMarginAux;
        percentMarginTele = mFovControlConfig.percentMarginMain;
    }

    mFovControlData.transitionParams.cropRatio = mFovControlData.basicFovRatio;

    mFovControlData.transitionParams.cutOverWideToTele =
            mFovControlData.transitionParams.cutOverFactor +
            (mFovControlConfig.percentMarginHysterisis / 100.0) * mFovControlData.basicFovRatio;

    mFovControlData.transitionParams.cutOverTeleToWide =
            mFovControlData.transitionParams.cutOverFactor;

    mFovControlData.transitionParams.transitionHigh =
            mFovControlData.transitionParams.cutOverWideToTele +
            (percentMarginWide / 100.0) * mFovControlData.basicFovRatio;

    mFovControlData.transitionParams.transitionLow =
            mFovControlData.transitionParams.cutOverTeleToWide -
            (percentMarginTele / 100.0) * mFovControlData.basicFovRatio;

    if (mFovControlConfig.snapshotPPConfig.enablePostProcess) {
        // Expand the transition zone if necessary to account for
        // the snapshot post-process settings
        if (mFovControlConfig.snapshotPPConfig.zoomMax >
                mFovControlData.transitionParams.transitionHigh) {
            mFovControlData.transitionParams.transitionHigh =
                mFovControlConfig.snapshotPPConfig.zoomMax;
        }
        if (mFovControlConfig.snapshotPPConfig.zoomMin <
                mFovControlData.transitionParams.transitionLow) {
            mFovControlData.transitionParams.transitionLow =
                mFovControlConfig.snapshotPPConfig.zoomMax;
        }

        // Set aux switch brightness threshold as the lower of aux switch and
        // snapshot post-process thresholds
        if (mFovControlConfig.snapshotPPConfig.luxMin < mFovControlConfig.auxSwitchBrightnessMin) {
            mFovControlConfig.auxSwitchBrightnessMin = mFovControlConfig.snapshotPPConfig.luxMin;
        }
    }

    LOGD("transition param: TransitionLow  %f", mFovControlData.transitionParams.transitionLow);
    LOGD("transition param: TeleToWide     %f", mFovControlData.transitionParams.cutOverTeleToWide);
    LOGD("transition param: WideToTele     %f", mFovControlData.transitionParams.cutOverWideToTele);
    LOGD("transition param: TransitionHigh %f", mFovControlData.transitionParams.transitionHigh);
}


/*===========================================================================
 * FUNCTION   : findZoomValue
 *
 * DESCRIPTION: For the input zoom ratio, find the zoom value.
 *              Zoom table contains zoom ratios where the indices
 *              in the zoom table indicate the corresponding zoom values.
 * PARAMETERS :
 * @zoomRatio : Zoom ratio
 *
 * RETURN     : Zoom value
 *
 *==========================================================================*/
uint32_t QCameraFOVControl::findZoomValue(
        uint32_t zoomRatio)
{
    uint32_t zoom = 0;
    for (uint32_t i = 0; i < mFovControlData.zoomRatioTableCount; ++i) {
        if (zoomRatio <= mFovControlData.zoomRatioTable[i]) {
            zoom = i;
            break;
        }
    }
    return zoom;
}


/*===========================================================================
 * FUNCTION   : findZoomRatio
 *
 * DESCRIPTION: For the input zoom value, find the zoom ratio.
 *              Zoom table contains zoom ratios where the indices
 *              in the zoom table indicate the corresponding zoom values.
 * PARAMETERS :
 * @zoom      : zoom value
 *
 * RETURN     : zoom ratio
 *
 *==========================================================================*/
uint32_t QCameraFOVControl::findZoomRatio(
        uint32_t zoom)
{
    return mFovControlData.zoomRatioTable[zoom];
}


/*===========================================================================
 * FUNCTION   : readjustZoomForTele
 *
 * DESCRIPTION: Calculate the zoom value for the tele camera based on zoom value
 *              for the wide camera
 *
 * PARAMETERS :
 * @zoomWide  : Zoom value for wide camera
 *
 * RETURN     : Zoom value for tele camera
 *
 *==========================================================================*/
uint32_t QCameraFOVControl::readjustZoomForTele(
        uint32_t zoomWide)
{
    uint32_t zoomRatioWide;
    uint32_t zoomRatioTele;

    zoomRatioWide = findZoomRatio(zoomWide);
    zoomRatioTele  = zoomRatioWide / mFovControlData.transitionParams.cutOverFactor;

    return(findZoomValue(zoomRatioTele));
}


/*===========================================================================
 * FUNCTION   : readjustZoomForWide
 *
 * DESCRIPTION: Calculate the zoom value for the wide camera based on zoom value
 *              for the tele camera
 *
 * PARAMETERS :
 * @zoomTele  : Zoom value for tele camera
 *
 * RETURN     : Zoom value for wide camera
 *
 *==========================================================================*/
uint32_t QCameraFOVControl::readjustZoomForWide(
        uint32_t zoomTele)
{
    uint32_t zoomRatioWide;
    uint32_t zoomRatioTele;

    zoomRatioTele = findZoomRatio(zoomTele);
    zoomRatioWide = zoomRatioTele * mFovControlData.transitionParams.cutOverFactor;

    return(findZoomValue(zoomRatioWide));
}


/*===========================================================================
 * FUNCTION   : convertUserZoomToWideAndTele
 *
 * DESCRIPTION: Calculate the zoom value for the wide and tele cameras
 *              based on the input user zoom value
 *
 * PARAMETERS :
 * @zoom      : User zoom value
 *
 * RETURN     : none
 *
 *==========================================================================*/
void QCameraFOVControl::convertUserZoomToWideAndTele(
        uint32_t zoom)
{
    Mutex::Autolock lock(mMutex);

    mFovControlData.zoomWide = zoom;
    mFovControlData.zoomTele  = readjustZoomForTele(mFovControlData.zoomWide);
}


/*===========================================================================
 * FUNCTION   : convertDisparityForInputParams
 *
 * DESCRIPTION: Convert the disparity for translation of input parameters
 *
 * PARAMETERS : none
 *
 * RETURN     : none
 *
 *==========================================================================*/
void QCameraFOVControl::convertDisparityForInputParams()
{
    float zoom = mFovControlData.zoomWide / (float)mFovControlData.zoomRatioTable[0];

    mFovControlData.shiftHorzAdjusted = (mFovControlData.transitionParams.cropRatio / zoom) *
            mFovControlData.spatialAlignResult.shiftTele.shiftHorz;
    mFovControlData.shiftVertAdjusted = (mFovControlData.transitionParams.cropRatio / zoom) *
            mFovControlData.spatialAlignResult.shiftTele.shiftVert;
}


/*===========================================================================
 * FUNCTION   : translateFocusAreas
 *
 * DESCRIPTION: Translate the focus areas from main to aux camera.
 *
 * PARAMETERS :
 * @roiAfMain : Focus area ROI for main camera
 *
 * RETURN     : Translated focus area ROI for aux camera
 *
 *==========================================================================*/
cam_roi_info_t QCameraFOVControl::translateFocusAreas(
        cam_roi_info_t roiAfMain)
{
    float fovRatio;
    float zoomMain;
    float zoomAux;
    float zoomWide;
    float zoomTele;
    float AuxDiffRoiLeft;
    float AuxDiffRoiTop;
    float AuxRoiLeft;
    float AuxRoiTop;
    cam_roi_info_t roiAfAux = roiAfMain;

    zoomWide = findZoomRatio(mFovControlData.zoomWide);
    zoomTele = findZoomRatio(mFovControlData.zoomTele);

    zoomMain = isMainCamFovWider() ? zoomWide : zoomTele;
    zoomAux  = isMainCamFovWider() ? zoomTele : zoomWide;

    if (isMainCamFovWider()) {
        fovRatio = (zoomAux / zoomMain) * mFovControlData.transitionParams.cropRatio;
    } else {
        fovRatio = (zoomAux / zoomMain) * (1.0f / mFovControlData.transitionParams.cropRatio);
    }

    for (int i = 0; i < roiAfMain.num_roi; ++i) {
        AuxDiffRoiLeft = fovRatio * (roiAfMain.roi[i].left -
                            (mFovControlData.previewSize.width / 2));
        AuxRoiLeft = (mFovControlData.previewSize.width / 2) + AuxDiffRoiLeft;

        AuxDiffRoiTop = fovRatio * (roiAfMain.roi[i].top -
                            (mFovControlData.previewSize.height/ 2));
        AuxRoiTop = (mFovControlData.previewSize.height / 2) + AuxDiffRoiTop;

        roiAfAux.roi[i].width  = roiAfMain.roi[i].width * fovRatio;
        roiAfAux.roi[i].height = roiAfMain.roi[i].height * fovRatio;

        roiAfAux.roi[i].left = AuxRoiLeft - mFovControlData.shiftHorzAdjusted;
        roiAfAux.roi[i].top  = AuxRoiTop - mFovControlData.shiftVertAdjusted;

        // Check the ROI bounds and correct if necessory
        // If ROI is out of bounds, revert to default ROI
        if ((roiAfAux.roi[i].left >= mFovControlData.previewSize.width) ||
            (roiAfAux.roi[i].top >= mFovControlData.previewSize.height)) {
            // TODO : use default ROI when available from AF. This part of the code
            // is still being worked upon. WA - set it to main cam ROI
            roiAfAux = roiAfMain;
            LOGW("AF ROI translation failed, reverting to the default ROI");
        } else {
            if (roiAfAux.roi[i].left < 0) {
                roiAfAux.roi[i].left = 0;
                LOGW("AF ROI translation failed");
            }
            if (roiAfAux.roi[i].top < 0) {
                roiAfAux.roi[i].top = 0;
                LOGW("AF ROI translation failed");
            }
            if ((roiAfAux.roi[i].left + roiAfAux.roi[i].width) >=
                        mFovControlData.previewSize.width) {
                roiAfAux.roi[i].width = mFovControlData.previewSize.width - roiAfAux.roi[i].left;
                LOGW("AF ROI translation failed");
            }
            if ((roiAfAux.roi[i].top + roiAfAux.roi[i].height) >=
                        mFovControlData.previewSize.height) {
                roiAfAux.roi[i].height = mFovControlData.previewSize.height - roiAfAux.roi[i].top;
                LOGW("AF ROI translation failed");
            }
        }
    }
    return roiAfAux;
}


/*===========================================================================
 * FUNCTION   : translateMeteringAreas
 *
 * DESCRIPTION: Translate the AEC metering areas from main to aux camera.
 *
 * PARAMETERS :
 * @roiAfMain : AEC ROI for main camera
 *
 * RETURN     : Translated AEC ROI for aux camera
 *
 *==========================================================================*/
cam_set_aec_roi_t QCameraFOVControl::translateMeteringAreas(
        cam_set_aec_roi_t roiAecMain)
{
    float fovRatio;
    float zoomMain;
    float zoomAux;
    float zoomWide;
    float zoomTele;
    float AuxDiffRoiX;
    float AuxDiffRoiY;
    float AuxRoiX;
    float AuxRoiY;
    cam_set_aec_roi_t roiAecAux = roiAecMain;

    zoomWide = findZoomRatio(mFovControlData.zoomWide);
    zoomTele = findZoomRatio(mFovControlData.zoomTele);

    zoomMain = isMainCamFovWider() ? zoomWide : zoomTele;
    zoomAux  = isMainCamFovWider() ? zoomTele : zoomWide;

    if (isMainCamFovWider()) {
        fovRatio = (zoomAux / zoomMain) * mFovControlData.transitionParams.cropRatio;
    } else {
        fovRatio = (zoomAux / zoomMain) * (1.0f / mFovControlData.transitionParams.cropRatio);
    }

    for (int i = 0; i < roiAecMain.num_roi; ++i) {
        AuxDiffRoiX = fovRatio * ((float)roiAecMain.cam_aec_roi_position.coordinate[i].x -
                          (mFovControlData.previewSize.width / 2));
        AuxRoiX = (mFovControlData.previewSize.width / 2) + AuxDiffRoiX;

        AuxDiffRoiY = fovRatio * ((float)roiAecMain.cam_aec_roi_position.coordinate[i].y -
                          (mFovControlData.previewSize.height / 2));
        AuxRoiY = (mFovControlData.previewSize.height / 2) + AuxDiffRoiY;

        roiAecAux.cam_aec_roi_position.coordinate[i].x = AuxRoiX +
                mFovControlData.shiftHorzAdjusted;
        roiAecAux.cam_aec_roi_position.coordinate[i].y = AuxRoiY +
                mFovControlData.shiftVertAdjusted;

        // Check the ROI bounds and correct if necessory
        if ((AuxRoiX < 0) ||
            (AuxRoiY < 0)) {
            roiAecAux.cam_aec_roi_position.coordinate[i].x = 0;
            roiAecAux.cam_aec_roi_position.coordinate[i].y = 0;
            LOGW("AEC ROI translation failed");
        } else if ((AuxRoiX >= mFovControlData.previewSize.width) ||
            (AuxRoiY >= mFovControlData.previewSize.height)) {
            // Clamp the Aux AEC ROI co-ordinates to max possible value
            if (AuxRoiX >= mFovControlData.previewSize.width) {
                roiAecAux.cam_aec_roi_position.coordinate[i].x =
                        mFovControlData.previewSize.width - 1;
            }
            if (AuxRoiY >= mFovControlData.previewSize.height) {
                roiAecAux.cam_aec_roi_position.coordinate[i].y =
                        mFovControlData.previewSize.height - 1;
            }
            LOGW("AEC ROI translation failed");
        }
    }
    return roiAecAux;
}


/*===========================================================================
 * FUNCTION   : translateRoiFD
 *
 * DESCRIPTION: Translate face detection ROI from aux metadata to main
 *
 * PARAMETERS :
 * @faceDetectionInfo  : face detection data from aux metadata. This face
 *                       detection data is overwritten with the translated
 *                       FD ROI.
 *
 * RETURN     : none
 *
 *==========================================================================*/
cam_face_detection_data_t QCameraFOVControl::translateRoiFD(
        cam_face_detection_data_t metaFD)
{
    cam_face_detection_data_t metaFDTranslated = metaFD;

    for (int i = 0; i < metaFDTranslated.num_faces_detected; ++i) {
        if (mDualCamParams.positionAux == CAM_POSITION_LEFT) {
            metaFDTranslated.faces[i].face_boundary.left -=
                mFovControlData.spatialAlignResult.shiftTele.shiftHorz;
        } else {
            metaFDTranslated.faces[i].face_boundary.left +=
                mFovControlData.spatialAlignResult.shiftTele.shiftHorz;
        }
    }
    return metaFDTranslated;
}
}; // namespace qcamera
