Description of Directory

```
.
└── source
    ├── client
    │   └── src              // Client source codes
    ├── common                // Common source codes used by client and server
    │   ├── keys              
    │   │   ├── ca          // CA. self signed root certificate.
    │   │   ├── client      // CA signed certificate & Private key for client
    │   │   └── server      // CA signed certificate & Private key for server
    │   └── libs              
    │       └── libcertcheck // It is certification check library and uses openssl v1.1.1 as the crypto library
    │                           // The root key for the crypto API are included as a string with obfuscated
    └── server                
        ├── facenetModels     // The path of faceNet models
        ├── imgs              // The path of the photo registered with name. The name and the photo are encrypted
        ├── mtCNNModels       // The path of machine learning model in MTCNN_FaceDetection_TensorRT
        ├── src               // Server source codes
        └── trt_l2norm_helper // TensorRT L2-Norm Helper
```
