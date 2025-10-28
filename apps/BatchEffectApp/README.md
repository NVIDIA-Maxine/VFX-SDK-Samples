BatchEffectApp
==============

BatchEffectApp is a sample application that demonstrates the simultaneous processing of a batch of images/videos by certain effects of the NVIDIA Video Effects SDK, in order to achieve higher performance. The supported features for this app are Artifact Reduction, Super Resolution and Upscaling. The batch script provided demonstrates the usage of multiple images as inputs, which are expected to be of the same resolution. These can be specified with command-line arguments enumerated by executing: `BatchEffectApp.exe --help` (on Windows) or `./BatchEffectApp --help` (on Linux). 

Similar to BatchEffectApp, we have also provided BatchDenoiseEffectApp and BatchAigsEffectApp, which demonstrates batching in the Webcam Denoising and AI Green Screen features respectively. The batch script provided demonstrates the usage of multiple videos as inputs, which are expected to be of the same resolution and length.


Required Features
-----------------
This app requires the following features to be installed. Make sure to install them using *install_features.ps1* (Windows) or *install_features.sh* (Linux) in your VFX SDK features directory before building it.
- nvVFXArtifactReduction
- nvVFXSuperRes
- nvVFXUpscale
- nvVFXTransfer

BatchEffectApp Command-Line Reference
-------------------------------------

| Flag                | Description |
|---------------------|-------------|
| `--out_file=<path>` | Output video files to be written. Specify a pattern that includes `%u` or `%d`. Default is `BatchOut_%02u.mp4`. |
| `--effect=<effect>` | One of the following effects to be applied:<br><br>- Transfer<br>- ArtifactReduction<br>- SuperRes<br>- Upscale |
| `--strength=<value>` | The strength of an effect, specified as a float value.<br><br>- For upscaling, the range is `0.0` to `1.0`.<br>- For super resolution, the typical range is `0.0` to `4.0`. |
| `--scale=<scale>`   | The scale factor to be applied. Valid values are `1.3333333`, `1.5`, `2`, `3`, or `4`. |
| `--mode=<mode>`     | For SuperRes or ArtifactReduction, selects the strength of the filter to be applied.<br><br>- `0`: Weak effect.<br>- `1`: Strong effect. |
| `--model_dir=<path>` | The path to the directory that contains the models. |
| `--verbose`         | Shows verbose output. |
| `--help`            | Displays help information. |
| `--log=<file>`      | Log SDK errors to a file, "stderr" (default), or "". |
| `--log_level=<n>`   | The desired log level: `0` (fatal), `1` (error; default), `2` (warning), or `3` (info). |


BatchAigsEffectApp Command-Line Reference
-----------------------------------------

Here is the command-line reference information for the BatchAigsEffectApp application.

**Usage:** `BatchAigsEffectApp [<flags>...] <inFile1> [<inFileN>...]`

Here are the flags:

| Flag                | Description |
|---------------------|-------------|
| `--out_file=<path>` | Output video files to be written. Specify a pattern that includes `%u` or `%d`. Default is `BatchOut_%02u.mp4`. |
| `--model_dir=<path>` | The path to the directory that contains the models. |
| `--mode=<value>`    | Selects the mode in which to run the application:<br><br>- `0`: Best quality.<br>- `1`: Fastest performance. |
| `--verbose`         | Shows verbose output. |
| `--codec=<fourcc>`  | The four-character code (FourCC) for the desired codec. For example, `avc1` or `h264`. |
| `--help`            | Displays help information. |
| `--log=<file>`      | Log SDK errors to a file, "stderr" (default), or "". |
| `--log_level=<n>`   | The desired log level: `0` (fatal), `1` (error; default), `2` (warning), or `3` (info). |


BatchDenoiseEffectApp Command-Line Reference
--------------------------------------------

Here is the command-line reference information for the BatchDenoiseEffectApp application.

**Usage:** `BatchDenoiseEffectApp [<flags>...] <inFile1> [<inFileN>...]`

Here are the flags:

| Flag                  | Description |
|-----------------------|-------------|
| `--out_file=<path>`   | Output video files to be written. Specify a pattern that includes `%u` or `%d`. Default is `BatchOut_%02u.mp4`. |
| `--strength=<value>`  | The strength of the effect:<br><br>- `0`: Weak effect.<br>- `1`: Strong effect. |
| `--batchsize=<value>` | The size of the batch (default `8`). |
| `--model_dir=<path>`  | The path to the directory that contains the models. |
| `--verbose`           | Shows verbose output. |
| `--help`              | Displays help information. |
| `--log=<file>`        | Log SDK errors to a file, "stderr" (default), or "". |
| `--log_level=<n>`     | The desired log level: `0` (fatal), `1` (error; default), `2` (warning), or `3` (info). |


Batching
--------

Some of the Video Effects have higher performance when multiple images are submitted in a contiguous batch.
All video effects can process batches, regardless of whether the have a specifically-tuned model.

Submitting a batch differs from submitting a single image in several ways:

1) Allocation of batched buffers
2) Choosing a batched model
3) Setting the batched images
4) Setting the batch size for Run().


What is a batch?
----------------

In the Video Effects SDK, a batch is a contiguous buffer containing multiple images with the same structure.
For some Effects, this can yield higher a throughput than submitting the images individually.

The batch is represented by an NvCVImage descriptor for the first image plus a separate batch size parameter that is set using

```
   NvVFX_SetU32(effect, NVVFX_BATCH_SIZE, batchSize);
```

This value is sampled every time `NvVFX_Run()` is called, thus allowing the batch size to change on every call to `NvVFX_Run()`.


Batch Utilities
---------------

There are several utility functions in BatchUtilities.cpp that facilitate working with image batches:
* `AllocateBatchBuffer()` can be used to allocate a buffer for a batch of images.
* `NthImage()` can be used to set a view into the nth image in a batched buffer.
* `ComputeImageBytes()` can be used to determine the number of bytes for each image, in order to advance the pixel pointer from one image to the next.
* `TransferToNthImage()` makes it easy to call `NvCVImage_Transfer` to set one of the images in a batch.
* `TransferFromNthImage()` makes it easy to call `NvCVImage_Transfer` to copy one of the images in a batch to a regular image.
* `TransferToBatchImage()` transfers multiple images from different locations to a batched image.
* `TransferFromBatchImage()` can be used to retrieve all of the images in a batch to diffent images in different locations.
The last two can also be accomplished by calling the Nth image APIs repeatedly,
but the source code illustrates an alternative method of accessing images in a batch.


Allocation of batched buffers
-----------------------------

Call the function `AllocateBatchBuffer()`. Note that this will allocate an image that is N times taller than the prototypical image,
though it cannot in general be interpreted as such, especially if the pixels are PLANAR.
Its purpose is only to provide storage, and to dispose of that storage when the `NvCVImage` goes out of scope or its destructor is called.

Note that you can use your own method to allocate the storage for the batched images.
The image yielded by `AllocateBatchBuffer()` is never used in any of the Video Effects APIs; it is only used for bookkeeping.
The APIs in VideoEffects only require an `NvCVImage` descriptor for the first image.


Choosing a Batch Model
----------------------

The Video Effects are comprised of several runtime engines that not only

1. implement a particular effect, but are also
2. tuned to take best advantage of a particular GPU architecture, and are
3. optimized to process multiple inputs simultaneously.

These "multiple inputs" we call a "batch", and the engine that is optimized
to process N images simultaneously is called a "Batch-N model".
A particular Effect on a given GPU architecture may have, 1, 2, or more batch models.

Note that the batch size of *images* submitted to Video Effects is unrelated to
the [maximum] batch size of a particular batch *model*, other than possibly efficiency.

It is expected that the maximum efficiency is achieved for image batch sizes
that are integral multiples of the model batch size,
e.g. a batch-4 model should be most efficient when passed image batches of size 4, 8, 12, ...,
though it is still possible to feed it batches of 3, 5, 10 or even 1.

All effects come with a model that is optimized for 1 image, i.e. a batch-1 model.
But some Effects may have other models that can process
larger batches of images simultaneously and efficiently.
If your application can take advantage of the higher efficiency of these larger batches,
you can specify your desired batch model before Loading it.

By default, a batch-1 model is loaded when `NvVFX_Load()` is called.
To choose another model, call

```
NvVFX_SetU32(effect, NVVFX_MODEL_BATCH, modelBatch);
```

with the desired model batch size *before* calling

```
NvVFX_Load(effect);
```

If the model with the desired batch size is available, `NVCV_SUCCESS` is returned.
Otherwise, an appropriate substitution will be made,
and the `NVCV_ERR_MODELSUBSTITUTION` status will be returned.
Note that `NVCV_ERR_MODELSUBSTITUTION` is not an error, but is instead a notification.
The most efficient batch model for your specified batch size has been loaded,
though it is not exactly what you asked for.

The batch size of the loaded model can be queried subsequently with

```
NvVFX_GetU32(effect, NVVFX_MODEL_BATCH, &modelBatch);
```

To find the available model batch sizes, query the INFO string:

```
NvVFX_GetString(effect, NVVFX_INFO, &infoStr);
```

though this INFO string is designed to be humanly parsable.

Programatically, you can call

```
NvVFX_SetU32(effect, NVVFX_MODEL_BATCH, modelBatch);
NvVFX_Load(effect);
NvVFX_GetU32(effect, NVVFX_MODEL_BATCH, &modelBatch);
```

repeatedly with increasing sizes, until the returned size is smaller than the requested one.
The values returned by `NvVFX_GetU32()` are exactly the available model batch sizes.
This information may be useful at runtime startup to maximize throughput and minimize latency.

Beware, though that `NvCV_Load()` is a heavyweight operation (on the order of seconds),
thus repeated calls should be avoided when the service is online.
The above programmatic example is merely suggested for offline querying purposes,
during startup or perhaps quarterly tune-ups.


Setting the Batched Images
--------------------------

As noted above, the API only takes the image descriptors for the first image in a batch.
The code below allocates the src and dst batch buffers,
and sets the input and outputs via the views of the first image in each batch buffer.
```
NvCVImage srcBatch, dstBatch, nthSrc, nthDst;                           // decl
AllocateBatchBuffer(&srcBatch, batchSize, srcWidth, srcHeight, ...);    // src batch
AllocateBatchBuffer(&dstBatch, batchSize, dstWidth, dstHeight, ...);    // dst batch
NthImage(0, srcHeight, &srcBatch, &nthSrc);                             // src proto
NthImage(0, dstHeight, &dstBatch, &nthDst);                             // dst proto
NvVFX_SetImage(effect, NVVFX_INPUT_IMAGE,  &nthSrc);                    // effect src
NvVFX_SetImage(effect, NVVFX_OUTPUT_IMAGE, &nthDst);                    // effect dst
```

or simply,

```
NvCVImage srcBatch, dstBatch, nth;
AllocateBatchBuffer(&srcBatch, batchSize, srcWidth, srcHeight, ...);
AllocateBatchBuffer(&dstBatch, batchSize, dstWidth, dstHeight, ...);
NvVFX_SetImage(effect, NVVFX_INPUT_IMAGE,  NthImage(0, srcHeight, &srcBatch, &nth));
NvVFX_SetImage(effect, NVVFX_OUTPUT_IMAGE, NthImage(0, dstHeight, &dstBatch, &nth));
```

since the image descriptors are copied into the Video Effects SDK.

All of the other images in the batch are computed
by advancing the pixel pointer by the size of each image.

The other aspect of setting the batched images is how to set the pixel values?
Each image in the batch is accessible with the `NthImage` function:

```
NthImage(n, imageHeight, &batchImage, &nthImage);
```

Then it is possible to use the same techniques used for other NvCVImages on the so-initialized nthImage view.
As suggested, `NthImage()` is just a thin wrapper around `NvCVImage_InitView()`, and can be used instead.
The `Transfer()` functions in BatchUtilities.cpp could be used to copy pixels to and from the batch.


Setting the Batch Size for Run()
--------------------------------

By default, the batch size processed by `Run()` is only 1.
To increase it to any number, call

```
NvVFX_SetU32(effect, NVVFX_BATCH_SIZE, batchSize);
```

before calling `Run()` in order for it to process a batch of that size.
As previously noted, there is no connection between the `MODEL_BATCH` and the `BATCH_SIZE`,
with the exception that the highest performance is expected to be when `BATCH_SIZE`
is an integral multiple of `MODEL_BATCH`. All images in the submitted batch will be processed.
