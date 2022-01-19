	nncase ir1.2.0:û

¥
*detection_layer_30/BiasAdd/conv/dequantize!detection_layer_30/BiasAdd/out_tp!detection_layer_30/BiasAdd/out_tp"	Transpose*
module_typeJstackvm*
actionJtrue
”
k210_0*detection_layer_30/BiasAdd/conv/dequantize*detection_layer_30/BiasAdd/conv/dequantize"
Dequantize*
module_typeJstackvm*
actionJtrue
£
input_12mobilenet_0.75_224/conv1_relu/Relu6/in_tp/quantize2mobilenet_0.75_224/conv1_relu/Relu6/in_tp/quantize"Quantize*
module_typeJstackvm*
actionJtrue
½
2mobilenet_0.75_224/conv1_relu/Relu6/in_tp/quantize)mobilenet_0.75_224/conv1_relu/Relu6/in_tp)mobilenet_0.75_224/conv1_relu/Relu6/in_tp"	Transpose*
module_typeJstackvm*
actionJtrue
i
)mobilenet_0.75_224/conv1_relu/Relu6/in_tpk210_0k210_0"Call*
module_typeJstackvm*
actionJtrue
•
!detection_layer_30/BiasAdd/out_tpdetection_layer_30/BiasAdd&detection_layer_30/BiasAdd/out_tp/copy"Copy*
module_typeJstackvm*
actionJtruemainZ#
input_1


à
À
b4
detection_layer_30/BiasAdd





j;
!detection_layer_30/BiasAdd/out_tp





jD
*detection_layer_30/BiasAdd/conv/dequantize





jN
2mobilenet_0.75_224/conv1_relu/Relu6/in_tp/quantize


à
À
jE
)mobilenet_0.75_224/conv1_relu/Relu6/in_tp



à
Àj 
k210_0





j4
detection_layer_30/BiasAdd





