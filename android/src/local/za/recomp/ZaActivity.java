package local.za.recomp;

import org.libsdl.app.SDLActivity;

/** ZA Recomp's runtime (libmain.so) with the libraries it links against, loaded in order. */
public class ZaActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] {
            "SDL3", "avutil", "swscale", "avcodec", "avformat", "shaderc_shared", "main"
        };
    }
}
