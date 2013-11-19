package rtbkit;

public class Object {
    static {
        System.loadLibrary("rtbkit_java");
    }

    protected Object(long handle) {
        this.handle = handle;
    }

    public void close() {
        if(handle != 0) {
            release(handle);
            handle = 0;
        }
    }

    private long handle;

    private native void release(long handle);
}

