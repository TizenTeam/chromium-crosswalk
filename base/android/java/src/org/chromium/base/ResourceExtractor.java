// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.annotation.TargetApi;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.res.AssetManager;
import android.os.AsyncTask;
import android.os.Build;
import android.os.Trace;
import android.preference.PreferenceManager;
import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.FilenameFilter;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.Set;
import java.util.List;
import java.util.concurrent.CancellationException;
import java.util.concurrent.ExecutionException;
import java.util.regex.Pattern;

/**
 * Handles extracting the necessary resources bundled in an APK and moving them to a location on
 * the file system accessible from the native code.
 */
public class ResourceExtractor {

    private static final String LOGTAG = "ResourceExtractor";
    private static final String LAST_LANGUAGE = "Last language";
    private static final String PAK_FILENAMES = "Pak filenames";
    private static final String ICU_DATA_FILENAME = "icudtl.dat";
    private static final String V8_NATIVES_DATA_FILENAME = "natives_blob.bin";
    private static final String V8_SNAPSHOT_DATA_FILENAME = "snapshot_blob.bin";

    private static String[] sMandatoryPaks = null;
    private static ResourceIntercepter sIntercepter = null;

    // By default, we attempt to extract a pak file for the users
    // current device locale. Use setExtractImplicitLocale() to
    // change this behavior.
    private static boolean sExtractImplicitLocalePak = true;

    public interface ResourceIntercepter {
        Set<String> getInterceptableResourceList();
        InputStream interceptLoadingForResource(String resource);
    }

    private class ExtractTask extends AsyncTask<Void, Void, Void> {
        private static final int BUFFER_SIZE = 16 * 1024;

        public ExtractTask() {
        }

        private void doInBackgroundImpl() {
            final File outputDir = getOutputDir();
            if (!outputDir.exists() && !outputDir.mkdirs()) {
                Log.e(LOGTAG, "Unable to create pak resources directory!");
                return;
            }

            String timestampFile = null;
            beginTraceSection("checkPakTimeStamp");
            try {
                timestampFile = checkPakTimestamp(outputDir);
            } finally {
                endTraceSection();
            }
            if (timestampFile != null) {
                deleteFiles();
            }

            SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(
                    mContext.getApplicationContext());
            HashSet<String> filenames = (HashSet<String>) prefs.getStringSet(
                    PAK_FILENAMES, new HashSet<String>());
            String currentLocale = LocaleUtils.getDefaultLocale();
            String currentLanguage = currentLocale.split("-", 2)[0];

            if (prefs.getString(LAST_LANGUAGE, "").equals(currentLanguage)
                    && filenames.size() >= sMandatoryPaks.length) {
                boolean filesPresent = true;
                for (String file : filenames) {
                    if (!new File(outputDir, file).exists()) {
                        filesPresent = false;
                        break;
                    }
                }
                if (filesPresent) return;
            } else {
                prefs.edit().putString(LAST_LANGUAGE, currentLanguage).apply();
            }

            StringBuilder p = new StringBuilder();
            for (String mandatoryPak : sMandatoryPaks) {
                if (p.length() > 0) p.append('|');
                p.append("\\Q" + mandatoryPak + "\\E");
            }

            if (sExtractImplicitLocalePak) {
                if (p.length() > 0) p.append('|');
                // As well as the minimum required set of .paks above, we'll
                // also add all .paks that we have for the user's currently
                // selected language.
                p.append(currentLanguage);
                p.append("(-\\w+)?\\.pak");
            }

            Pattern paksToInstall = Pattern.compile(p.toString());

            AssetManager manager = mContext.getResources().getAssets();
            beginTraceSection("WalkAssets");
            try {
                // Loop through every asset file that we have in the APK, and look for the
                // ones that we need to extract by trying to match the Patterns that we
                // created above.
                byte[] buffer = null;
                String[] files = manager.list("");
                if (sIntercepter != null) {
                    Set<String> filesIncludingInterceptableFiles =
                            sIntercepter.getInterceptableResourceList();
                    if (filesIncludingInterceptableFiles != null &&
                            !filesIncludingInterceptableFiles.isEmpty()) {
                        for (String file : files) {
                            filesIncludingInterceptableFiles.add(file);
                        }
                        files = new String[filesIncludingInterceptableFiles.size()];
                        filesIncludingInterceptableFiles.toArray(files);
                    }
                }
                for (String file : files) {
                    if (!paksToInstall.matcher(file).matches()) {
                        continue;
                    }
                    boolean isAppDataFile = file.equals(ICU_DATA_FILENAME)
                            || file.equals(V8_NATIVES_DATA_FILENAME)
                            || file.equals(V8_SNAPSHOT_DATA_FILENAME);
                    File output = new File(isAppDataFile ? getAppDataDir() : outputDir, file);
                    if (output.exists()) {
                        continue;
                    }

                    InputStream is = null;
                    OutputStream os = null;
                    beginTraceSection("ExtractResource");
                    try {
                        if (sIntercepter != null) {
                            is = sIntercepter.interceptLoadingForResource(file);
                        }
                        if (is == null) is = manager.open(file);
                        os = new FileOutputStream(output);
                        Log.i(LOGTAG, "Extracting resource " + file);
                        if (buffer == null) {
                            buffer = new byte[BUFFER_SIZE];
                        }

                        int count = 0;
                        while ((count = is.read(buffer, 0, BUFFER_SIZE)) != -1) {
                            os.write(buffer, 0, count);
                        }
                        os.flush();

                        // Ensure something reasonable was written.
                        if (output.length() == 0) {
                            throw new IOException(file + " extracted with 0 length!");
                        }

                        if (!isAppDataFile) {
                            filenames.add(file);
                        } else {
                            // icu and V8 data need to be accessed by a renderer
                            // process.
                            output.setReadable(true, false);
                        }
                    } finally {
                        try {
                            if (is != null) {
                                is.close();
                            }
                        } finally {
                            if (os != null) {
                                os.close();
                            }
                            endTraceSection(); // ExtractResource
                        }
                    }
                }
            } catch (IOException e) {
                // TODO(benm): See crbug/152413.
                // Try to recover here, can we try again after deleting files instead of
                // returning null? It might be useful to gather UMA here too to track if
                // this happens with regularity.
                Log.w(LOGTAG, "Exception unpacking required pak resources: " + e.getMessage());
                deleteFiles();
                return;
            } finally {
                endTraceSection(); // WalkAssets
            }

            // Finished, write out a timestamp file if we need to.
            if (timestampFile != null) {
                try {
                    new File(outputDir, timestampFile).createNewFile();
                } catch (IOException e) {
                    // Worst case we don't write a timestamp, so we'll re-extract the resource
                    // paks next start up.
                    Log.w(LOGTAG, "Failed to write resource pak timestamp!");
                }
            }
            // TODO(yusufo): Figure out why remove is required here.
            prefs.edit().remove(PAK_FILENAMES).apply();
            prefs.edit().putStringSet(PAK_FILENAMES, filenames).apply();
        }

        @Override
        protected Void doInBackground(Void... unused) {
            // TODO(lizeb): Use chrome tracing here (and above in
            // doInBackgroundImpl) when it will be possible. This is currently
            // not doable since the native library is not loaded yet, and the
            // TraceEvent calls are dropped before this point.
            beginTraceSection("ResourceExtractor.ExtractTask.doInBackground");
            try {
                doInBackgroundImpl();
            } finally {
                endTraceSection();
            }
            return null;
        }

        // Looks for a timestamp file on disk that indicates the version of the APK that
        // the resource paks were extracted from. Returns null if a timestamp was found
        // and it indicates that the resources match the current APK. Otherwise returns
        // a String that represents the filename of a timestamp to create.
        // Note that we do this to avoid adding a BroadcastReceiver on
        // android.content.Intent#ACTION_PACKAGE_CHANGED as that causes process churn
        // on (re)installation of *all* APK files.
        private String checkPakTimestamp(File outputDir) {
            final String timestampPrefix = "pak_timestamp-";
            PackageManager pm = mContext.getPackageManager();
            PackageInfo pi = null;

            try {
                pi = pm.getPackageInfo(mContext.getPackageName(), 0);
            } catch (PackageManager.NameNotFoundException e) {
                return timestampPrefix;
            }

            if (pi == null) {
                return timestampPrefix;
            }

            String expectedTimestamp = timestampPrefix + pi.versionCode + "-" + pi.lastUpdateTime;

            String[] timestamps = outputDir.list(new FilenameFilter() {
                @Override
                public boolean accept(File dir, String name) {
                    return name.startsWith(timestampPrefix);
                }
            });

            if (timestamps.length != 1) {
                // If there's no timestamp, nuke to be safe as we can't tell the age of the files.
                // If there's multiple timestamps, something's gone wrong so nuke.
                return expectedTimestamp;
            }

            if (!expectedTimestamp.equals(timestamps[0])) {
                return expectedTimestamp;
            }

            // timestamp file is already up-to date.
            return null;
        }

        @TargetApi(Build.VERSION_CODES.JELLY_BEAN_MR2)
        private void beginTraceSection(String section) {
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.JELLY_BEAN_MR2) return;
            Trace.beginSection(section);
        }

        @TargetApi(Build.VERSION_CODES.JELLY_BEAN_MR2)
        private void endTraceSection() {
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.JELLY_BEAN_MR2) return;
            Trace.endSection();
        }
    }

    private final Context mContext;
    private ExtractTask mExtractTask;

    private static ResourceExtractor sInstance;

    public static ResourceExtractor get(Context context) {
        if (sInstance == null) {
            sInstance = new ResourceExtractor(context);
        }
        return sInstance;
    }

    /**
     * Specifies the .pak files that should be extracted from the APK's asset resources directory
     * and moved to {@link #getOutputDirFromContext(Context)}.
     * @param mandatoryPaks The list of pak files to be loaded. If no pak files are
     *     required, pass a single empty string.
     */
    public static void setMandatoryPaksToExtract(String... mandatoryPaks) {
        assert (sInstance == null || sInstance.mExtractTask == null)
                : "Must be called before startExtractingResources is called";
        sMandatoryPaks = mandatoryPaks;

    }

    /**
     * Allow embedders to intercept the resource loading process. Embedders may
     * want to load paks from res/raw instead of assets, since assets are not
     * supported in Android library project.
     * @param intercepter The instance of intercepter which provides the files list
     * to intercept and the inputstream for the files it wants to intercept with.
     */
    public static void setResourceIntercepter(ResourceIntercepter intercepter) {
        assert (sInstance == null || sInstance.mExtractTask == null)
                : "Must be called before startExtractingResources is called";
        sIntercepter = intercepter;
    }

    /**
     * By default the ResourceExtractor will attempt to extract a pak resource for the users
     * currently specified locale. This behavior can be changed with this function and is
     * only needed by tests.
     * @param extract False if we should not attempt to extract a pak file for
     *         the users currently selected locale and try to extract only the
     *         pak files specified in sMandatoryPaks.
     */
    @VisibleForTesting
    public static void setExtractImplicitLocaleForTesting(boolean extract) {
        assert (sInstance == null || sInstance.mExtractTask == null)
                : "Must be called before startExtractingResources is called";
        sExtractImplicitLocalePak = extract;
    }

    /**
     * Marks all the 'pak' resources, packaged as assets, for extraction during
     * running the tests.
     */
    @VisibleForTesting
    public void setExtractAllPaksAndV8SnapshotForTesting() {
        List<String> pakAndSnapshotFileAssets = new ArrayList<String>();
        AssetManager manager = mContext.getResources().getAssets();
        try {
            String[] files = manager.list("");
            for (String file : files) {
                if (file.endsWith(".pak")) pakAndSnapshotFileAssets.add(file);
            }
        } catch (IOException e) {
            Log.w(LOGTAG, "Exception while accessing assets: " + e.getMessage(), e);
        }
        pakAndSnapshotFileAssets.add("natives_blob.bin");
        pakAndSnapshotFileAssets.add("snapshot_blob.bin");
        setMandatoryPaksToExtract(pakAndSnapshotFileAssets.toArray(
                new String[pakAndSnapshotFileAssets.size()]));
    }

    private ResourceExtractor(Context context) {
        mContext = context.getApplicationContext();
    }

    public void waitForCompletion() {
        if (shouldSkipPakExtraction()) {
            return;
        }

        assert mExtractTask != null;

        try {
            mExtractTask.get();
            // ResourceExtractor is not needed any more.
            // Release static objects to avoid leak of Context.
            sIntercepter = null;
            sInstance = null;
        } catch (CancellationException e) {
            // Don't leave the files in an inconsistent state.
            deleteFiles();
        } catch (ExecutionException e2) {
            deleteFiles();
        } catch (InterruptedException e3) {
            deleteFiles();
        }
    }

    /**
     * This will extract the application pak resources in an
     * AsyncTask. Call waitForCompletion() at the point resources
     * are needed to block until the task completes.
     */
    public void startExtractingResources() {
        if (mExtractTask != null) {
            return;
        }

        if (shouldSkipPakExtraction()) {
            return;
        }

        mExtractTask = new ExtractTask();
        mExtractTask.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    private File getAppDataDir() {
        return new File(PathUtils.getDataDirectory(mContext));
    }

    private File getOutputDir() {
        return new File(getAppDataDir(), "paks");
    }

    /**
     * Pak files (UI strings and other resources) should be updated along with
     * Chrome. A version mismatch can lead to a rather broken user experience.
     * Failing to update the V8 snapshot files will lead to a version mismatch
     * between V8 and the loaded snapshot which will cause V8 to crash, so this
     * is treated as an error. The ICU data (icudtl.dat) is less
     * version-sensitive, but still can lead to malfunction/UX misbehavior. So,
     * we regard failing to update them as an error.
     */
    private void deleteFiles() {
        File icudata = new File(getAppDataDir(), ICU_DATA_FILENAME);
        if (icudata.exists() && !icudata.delete()) {
            Log.e(LOGTAG, "Unable to remove the icudata " + icudata.getName());
        }
        File v8_natives = new File(getAppDataDir(), V8_NATIVES_DATA_FILENAME);
        if (v8_natives.exists() && !v8_natives.delete()) {
            Log.e(LOGTAG,
                    "Unable to remove the v8 data " + v8_natives.getName());
        }
        File v8_snapshot = new File(getAppDataDir(), V8_SNAPSHOT_DATA_FILENAME);
        if (v8_snapshot.exists() && !v8_snapshot.delete()) {
            Log.e(LOGTAG,
                    "Unable to remove the v8 data " + v8_snapshot.getName());
        }
        File dir = getOutputDir();
        if (dir.exists()) {
            File[] files = dir.listFiles();
            for (File file : files) {
                if (!file.delete()) {
                    Log.e(LOGTAG, "Unable to remove existing resource " + file.getName());
                }
            }
        }
    }

    /**
     * Pak extraction not necessarily required by the embedder; we allow them to skip
     * this process if they call setMandatoryPaksToExtract with a single empty String.
     */
    private static boolean shouldSkipPakExtraction() {
        // Must call setMandatoryPaksToExtract before beginning resource extraction.
        assert sMandatoryPaks != null;
        return sMandatoryPaks.length == 1 && "".equals(sMandatoryPaks[0]);
    }
}
