package rtbkit;

public class Handle {
    static {
        System.loadLibrary("rtbkit_java");
    }

    public Handle(String bootstrap) {
        handle = initialize(bootstrap);
    }

    public BiddingAgent makeBiddingAgent(String name) {
        return new BiddingAgent(this, name);
    }

    public void close() {
        if(handle != 0) {
            shutdown(handle);
            handle = 0;
        }
    }

    private long handle;

    private native long initialize(String bootstrap);
    private native void shutdown(long handle);

    public static void main(String[] args) {
        Handle root = new Handle("../../sample.bootstrap.json");
        root.close();
    }
}

