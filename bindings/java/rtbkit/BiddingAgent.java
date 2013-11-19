package rtbkit;

public class BiddingAgent extends Object {
    static {
        System.loadLibrary("rtbkit_java");
    }

    public BiddingAgent(Handle handle, String name) {
        super(createBiddingAgent(handle.handle, name));
    }

    private native void createBiddingAgent(long handle, String name);
}

