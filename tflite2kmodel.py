import nncase
import numpy as np
import cv2
import os
from torch.utils.data import Dataset, DataLoader

base_img_path = "smoke/JPEGImages/"


def read_model_file(model_file):
    with open(model_file, 'rb') as f:
        model_content = f.read()
    return model_content

class imgDataset(Dataset):
    def __init__(self):
        self.file_path = "smoke/JPEGImages"
        self.imgfiles = os.listdir(self.file_path)

    def __getitem__(self, index):
        img_path = base_img_path + self.imgfiles[index]
        img = cv2.imread(img_path)
        img = cv2.resize(img,(224,320))
        img = img.astype(np.float32)
        return img

    def __len__(self):
        return len(self.imgfiles)

def collate_fn(batch):

    batch = np.array(batch)
    return batch

def generate_data(batch_size):
    imgdataset = imgDataset()
    dataloader = DataLoader(imgdataset, batch_size=batch_size, shuffle=True,collate_fn=collate_fn)

    for i in dataloader:
        print(i.shape)
        return i



def main():
    model='2022-01-19_19-08-24/2022-01-19_19-08-24.tflite'
    input_shape = [1,224,320,3]
    target = 'k210'

    # compile_options
    compile_options = nncase.CompileOptions()
    compile_options.target = target
    compile_options.input_type = 'float32'
    compile_options.input_layout = 'NHWC'
    compile_options.output_layout = 'NHWC'
    compile_options.input_shape = input_shape
    compile_options.dump_ir = True
    compile_options.dump_asm = True
    compile_options.dump_dir = 'tmp'
    compile_options.quant_type = 'int8' # or 'int8'
    compile_options.w_quant_type = "int8"
    # compile_options.output_type = "uint8"
    compile_options.use_mse_quant_w = True
    compile_options.swapRB = True

    # compiler
    compiler = nncase.Compiler(compile_options)

    # import_options
    import_options = nncase.ImportOptions()

    # quantize model
    compile_options.quant_type = 'int8' # or 'int8'
    compile_options.w_quant_type = "int8"
    # compile_options.output_type = "uint8"
    # compile_options.use_mse_quant_w = True
    # compile_options.swapRB = True

    # ptq_options
    ptq_options = nncase.PTQTensorOptions()
    ptq_options.samples_count = 2
    ptq_options.set_tensor_data(generate_data(ptq_options.samples_count).tobytes())

    # import
    model_content = read_model_file(model)
    compiler.import_tflite(model_content, import_options)

    # compile
    compiler.use_ptq(ptq_options)
    compiler.compile()

    # kmodel
    kmodel = compiler.gencode_tobytes()
    with open('test.kmodel', 'wb') as f:
        f.write(kmodel)

if __name__ == '__main__':
    main()