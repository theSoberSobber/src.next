// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.app.Activity;
import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.download.DownloadLocationDialogMetrics.DownloadLocationSuggestionEvent;
import org.chromium.chrome.browser.download.dialogs.DownloadDialogUtils;
import org.chromium.chrome.browser.download.dialogs.DownloadLocationDialogController;
import org.chromium.chrome.browser.download.dialogs.DownloadLocationDialogCoordinator;
import org.chromium.chrome.browser.download.interstitial.NewDownloadTab;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.net.ConnectionType;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManagerHolder;

/**
 * Glues download dialogs UI code and handles the communication to download native backend.
 * When {@link ChromeFeatureList#DOWNLOAD_LATER} is enabled, the following dialogs will be shown in
 * a sequence.
 * Download later dialog ==> (optional) Download date time picker ==> Download location dialog
 * When {@link ChromeFeatureList#DOWNLOAD_LATER} is disabled, only the download location dialog will
 * be shown.
 */
public class DownloadDialogBridge implements DownloadLocationDialogController {
    private static final long INVALID_START_TIME = -1;
    private long mNativeDownloadDialogBridge;

    private final DownloadLocationDialogCoordinator mLocationDialog;

    private Context mContext;
    private ModalDialogManager mModalDialogManager;
    private WindowAndroid mWindowAndroid;
    private long mTotalBytes;
    private @ConnectionType int mConnectionType = ConnectionType.CONNECTION_NONE;
    private @DownloadLocationDialogType int mLocationDialogType;
    private String mSuggestedPath;
    private PrefService mPrefService;

    // Whether the user clicked the edit text to open download location dialog.
    private boolean mEditLocation;

    // Whether to show the edit location text in download later dialog.
    private boolean mShowEditLocation;

    public DownloadDialogBridge(
            long nativeDownloadDialogBridge, DownloadLocationDialogCoordinator locationDialog) {
        mNativeDownloadDialogBridge = nativeDownloadDialogBridge;
        mLocationDialog = locationDialog;
    }

    @CalledByNative
    private static DownloadDialogBridge create(long nativeDownloadDialogBridge) {
        DownloadLocationDialogCoordinator locationDialog = new DownloadLocationDialogCoordinator();
        DownloadDialogBridge bridge =
                new DownloadDialogBridge(nativeDownloadDialogBridge, locationDialog);
        locationDialog.initialize(bridge);
        return bridge;
    }

    @CalledByNative
    void destroy() {
        mNativeDownloadDialogBridge = 0;
        mLocationDialog.destroy();
    }

    @CalledByNative
    private void showDialog(WindowAndroid windowAndroid, long totalBytes,
            @ConnectionType int connectionType, @DownloadLocationDialogType int dialogType,
            String suggestedPath, boolean supportsLaterDialog) {
        Activity activity = windowAndroid.getActivity().get();
        if (activity == null) {
            onCancel();
            return;
        }

        DownloadDirectoryProvider.getInstance().getAllDirectoriesOptions((dirs) -> {
            mShowEditLocation = (dirs != null && dirs.size() > 1);
            ModalDialogManager modalDialogManager =
                    ((ModalDialogManagerHolder) activity).getModalDialogManager();

            // Suggests an alternative download location.
            @DownloadLocationDialogType
            int suggestedDialogType = dialogType;
            if (ChromeFeatureList.isEnabled(ChromeFeatureList.SMART_SUGGESTION_FOR_LARGE_DOWNLOADS)
                    && DownloadDialogUtils.shouldSuggestDownloadLocation(
                            dirs, getDownloadDefaultDirectory(), totalBytes)) {
                suggestedDialogType = DownloadLocationDialogType.LOCATION_SUGGESTION;
                DownloadLocationDialogMetrics.recordDownloadLocationSuggestionEvent(
                        DownloadLocationSuggestionEvent.LOCATION_SUGGESTION_SHOWN);
            }

            showDialog(activity, modalDialogManager, getPrefService(), totalBytes, connectionType,
                    suggestedDialogType, suggestedPath, supportsLaterDialog);
        });
    }

    @VisibleForTesting
    void showDialog(Context context, ModalDialogManager modalDialogManager, PrefService prefService,
            long totalBytes, @ConnectionType int connectionType,
            @DownloadLocationDialogType int dialogType, String suggestedPath,
            boolean supportsLaterDialog) {
        mContext = context;
        mModalDialogManager = modalDialogManager;
        mPrefService = prefService;

        mTotalBytes = totalBytes;
        mConnectionType = connectionType;
        mLocationDialogType = dialogType;
        mSuggestedPath = suggestedPath;

        mLocationDialog.showDialog(
                mContext, mModalDialogManager, totalBytes, dialogType, suggestedPath);
    }

    private void onComplete() {
        if (mNativeDownloadDialogBridge == 0) return;

        DownloadDialogBridgeJni.get().onComplete(mNativeDownloadDialogBridge,
                DownloadDialogBridge.this, mSuggestedPath, false, INVALID_START_TIME);
    }

    private void onCancel() {
        if (mNativeDownloadDialogBridge == 0) return;
        DownloadDialogBridgeJni.get().onCanceled(
                mNativeDownloadDialogBridge, DownloadDialogBridge.this);
        if (mWindowAndroid != null) {
            NewDownloadTab.closeExistingNewDownloadTab(mWindowAndroid);
            mWindowAndroid = null;
        }

        // The location dialog has error message text, show the location dialog after the download
        // later dialog.
        showLocationDialog(false /*editLocation*/);
    }

    @Override
    public void onDownloadLaterDialogCanceled() {
        DownloadLaterMetrics.recordDownloadLaterUiEvent(
                DownloadLaterUiEvent.DOWNLOAD_LATER_DIALOG_CANCEL);
        onCancel();
    }

    @Override
    public void onEditLocationClicked() {
        DownloadLaterMetrics.recordDownloadLaterUiEvent(
                DownloadLaterUiEvent.DOWNLOAD_LATER_DIALOG_EDIT_CLICKED);
        mDownloadLaterDialog.dismissDialog(DialogDismissalCause.ACTION_ON_CONTENT);

        // The user clicked the edit location text.
        showLocationDialog(true /* editLocation */);
    }

    private void showLocationDialog(boolean editLocation) {
        mEditLocation = editLocation;

        mDownloadLaterChoice = mDownloadLaterDialog.getChoice();

        mLocationDialog.showDialog(
                mContext, mModalDialogManager, mTotalBytes, mLocationDialogType, mSuggestedPath);
    }

    private void showDownloadLaterDialog() {
        assert mPrefService != null;
        @DownloadLaterPromptStatus
        int promptStatus = mPrefService.getInteger(Pref.DOWNLOAD_LATER_PROMPT_STATUS);
        PropertyModel.Builder builder =
                new PropertyModel.Builder(DownloadLaterDialogProperties.ALL_KEYS)
                        .with(DownloadLaterDialogProperties.CONTROLLER, mDownloadLaterDialog)
                        .with(DownloadLaterDialogProperties.INITIAL_CHOICE, mDownloadLaterChoice)
                        .with(DownloadLaterDialogProperties.DONT_SHOW_AGAIN_SELECTION, promptStatus)
                        .with(DownloadLaterDialogProperties.SUBTITLE_TEXT,
                                getDownloadLaterDialogSubtitle())
                        .with(DownloadLaterDialogProperties.SHOW_DATE_TIME_PICKER_OPTION,
                                DownloadDialogBridgeJni.get().shouldShowDateTimePicker());
        if (mShowEditLocation) {
            builder.with(DownloadLaterDialogProperties.LOCATION_TEXT,
                    mContext.getResources().getString(R.string.menu_downloads));
        }

        mDownloadLaterDialog.showDialog(
                mContext, mModalDialogManager, mPrefService, builder.build());
        DownloadLaterMetrics.recordDownloadLaterUiEvent(
                DownloadLaterUiEvent.DOWNLOAD_LATER_DIALOG_SHOW);
    }

    private String getDownloadLaterDialogSubtitle() {
        if (mConnectionType == ConnectionType.CONNECTION_2G) {
            return mContext.getResources().getString(R.string.download_later_slow_network_subtitle,
                    mContext.getResources().getString(R.string.download_later_2g_connection));
        }
        if (mConnectionType == ConnectionType.CONNECTION_BLUETOOTH) {
            return mContext.getResources().getString(R.string.download_later_slow_network_subtitle,
                    mContext.getResources().getString(
                            R.string.download_later_bluetooth_connection));
        }

        if (mTotalBytes >= DownloadDialogBridgeJni.get().getDownloadLaterMinFileSize()) {
            return mContext.getResources().getString(R.string.download_later_large_file_subtitle,
                    DownloadUtils.getStringForBytes(mContext, mTotalBytes));
        }

        return "";
    }

    // DownloadLocationDialogController implementation.
    @Override
    public void onDownloadLocationDialogComplete(String returnedPath) {
        mSuggestedPath = returnedPath;

        if (mLocationDialogType == DownloadLocationDialogType.LOCATION_SUGGESTION) {
            boolean isSelected = !mSuggestedPath.equals(getDownloadDefaultDirectory());
            DownloadLocationDialogMetrics.recordDownloadLocationSuggestionChoice(isSelected);
        }

        // The location dialog is triggered automatically, complete the flow.
        if (!mEditLocation) {
            onComplete();
            return;
        }

        // The location dialog is triggered by the "Edit" text. Show the download later dialog
        // again.
        mEditLocation = false;
    }

    @Override
    public void onDownloadLocationDialogCanceled() {
        if (!mEditLocation) {
            onCancel();
            return;
        }

        // The location dialog is triggered by the "Edit" text. Show the download later dialog
        // again.
        mEditLocation = false;
    }

    void setPrefServiceForTesting(PrefService prefService) {
        mPrefService = prefService;
    }

    /**
     * @return The stored download default directory.
     */
    public static String getDownloadDefaultDirectory() {
        return DownloadDialogBridgeJni.get().getDownloadDefaultDirectory();
    }

    /**
     * @param directory New directory to set as the download default directory.
     */
    public static void setDownloadAndSaveFileDefaultDirectory(String directory) {
        DownloadDialogBridgeJni.get().setDownloadAndSaveFileDefaultDirectory(directory);
    }

    /**
     * @return The status of prompt for download pref, defined by {@link DownloadPromptStatus}.
     */
    @DownloadPromptStatus
    public static int getPromptForDownloadAndroid() {
        return getPrefService().getInteger(Pref.PROMPT_FOR_DOWNLOAD_ANDROID);
    }

    /**
     * @param status New status to update the prompt for download preference.
     */
    public static void setPromptForDownloadAndroid(@DownloadPromptStatus int status) {
        getPrefService().setInteger(Pref.PROMPT_FOR_DOWNLOAD_ANDROID, status);
    }

    /**
     * @return The value for {@link Pref#PROMPT_FOR_DOWNLOAD}. This is currently only used by
     * enterprise policy.
     */
    public static boolean getPromptForDownloadPolicy() {
        return getPrefService().getBoolean(Pref.PROMPT_FOR_DOWNLOAD);
    }

    /**
     * @return whether to prompt the download location dialog is controlled by enterprise policy.
     */
    public static boolean isLocationDialogManaged() {
        return DownloadDialogBridgeJni.get().isLocationDialogManaged();
    }

    public static boolean shouldShowDateTimePicker() {
        return DownloadDialogBridgeJni.get().shouldShowDateTimePicker();
    }

    private static PrefService getPrefService() {
        return UserPrefs.get(Profile.getLastUsedRegularProfile());
    }

    @NativeMethods
    public interface Natives {
        void onComplete(long nativeDownloadDialogBridge, DownloadDialogBridge caller,
                String returnedPath, boolean onWifi, long startTime);
        void onCanceled(long nativeDownloadDialogBridge, DownloadDialogBridge caller);
        String getDownloadDefaultDirectory();
        void setDownloadAndSaveFileDefaultDirectory(String directory);
        long getDownloadLaterMinFileSize();
        boolean shouldShowDateTimePicker();
        boolean isLocationDialogManaged();
    }
}
