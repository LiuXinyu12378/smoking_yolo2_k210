import glob
import xml.etree.ElementTree as ET
from tqdm import tqdm
import numpy as np
import cv2
from kmeans import kmeans, avg_iou

# ANNOTATIONS_PATH = "Annotations"
CLUSTERS = 5


def load_dataset(path):
    dataset = []
    for xml_file in glob.glob("{}/*xml".format(path)):

        try:
            # print(xml_file)
            tree = ET.parse(xml_file)

            # height = int(tree.findtext("./size/height"))
            # width = int(tree.findtext("./size/width"))
            img_file = xml_file.replace("Annotations","JPEGImages").replace(".xml",".jpg")

            image = cv2.imread(img_file)
            height,width = image.shape[0],image.shape[1]

            for obj in tree.iter("object"):
                xmin = int(obj.findtext("bndbox/xmin")) / width
                ymin = int(obj.findtext("bndbox/ymin")) / height
                xmax = int(obj.findtext("bndbox/xmax")) / width
                ymax = int(obj.findtext("bndbox/ymax")) / height

                dataset.append([xmax - xmin, ymax - ymin])
        except:
            pass

    return np.array(dataset)


ANNOTATIONS_PATH = "smoke/Annotations"
data = load_dataset(ANNOTATIONS_PATH)
out = kmeans(data, k=CLUSTERS)
print("Accuracy: {:.2f}%".format(avg_iou(data, out) * 100))
# print("Boxes:\n {}".format(out))
print("Boxes:\n {}-{}".format(out[:, 0] * 224, out[:, 1] * 320))
ratios = np.around(out[:, 0] / out[:, 1], decimals=2).tolist()
print("Ratios:\n {}".format(sorted(ratios)))

print(sorted(zip(out[:, 0] * 224, out[:, 1] * 320)))