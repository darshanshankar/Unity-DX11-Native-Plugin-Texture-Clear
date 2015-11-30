using UnityEngine;
using System;
using System.Collections;
using System.Runtime.InteropServices;


public class UseRenderingPlugin : MonoBehaviour
{
    // Native plugin rendering events are only called if a plugin is used
    // by some script. This means we have to DllImport at least
    // one function in some active script.
    // For this example, we'll call into plugin's SetTimeFromUnity
    // function and pass the current time so the plugin can animate.

    [DllImport ("RenderingPlugin")]
    private static extern void SetTimeFromUnity(float t);


    // We'll also pass native pointer to a texture in Unity.
    // The plugin will fill texture data from native code.
    [DllImport ("RenderingPlugin")]
    private static extern void SetTextureFromUnity(System.IntPtr texture);


    [DllImport("RenderingPlugin")]
    private static extern void SetUnityStreamingAssetsPath([MarshalAs(UnmanagedType.LPStr)] string path);


    [DllImport("RenderingPlugin")]
    private static extern IntPtr GetRenderEventFunc();


    IEnumerator Start()
    {
        SetUnityStreamingAssetsPath(Application.streamingAssetsPath);

        CreateTextureAndPassToPlugin();
        yield return StartCoroutine("CallPluginAtEndOfFrames");
    }

    private void CreateTextureAndPassToPlugin()
    {
        // Create a texture
        RenderTexture tex = new RenderTexture(256,256,0,RenderTextureFormat.ARGB32,RenderTextureReadWrite.Default);
        // Set point filtering just so we can see the pixels clearly
        tex.filterMode = FilterMode.Point;
        // Call Apply() so it's actually uploaded to the GPU
        tex.Create();

        // Set texture onto our material
        GetComponent<Renderer>().material.mainTexture = tex;

        // Pass texture pointer to the plugin
        SetTextureFromUnity (tex.GetNativeTexturePtr());
    }

    private IEnumerator CallPluginAtEndOfFrames()
    {
        while (true) {
            // Wait until all frame rendering is done
            yield return new WaitForEndOfFrame();

            // Set time for the plugin
            SetTimeFromUnity (Time.timeSinceLevelLoad);

            // Issue a plugin event with arbitrary integer identifier.
            // The plugin can distinguish between different
            // things it needs to do based on this ID.
            // For our simple plugin, it does not matter which ID we pass here.
            GL.IssuePluginEvent(GetRenderEventFunc(), 1);
        }
    }
}
