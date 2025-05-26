

// GDALtest.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include "include/gdal.h"
#include "include/gdal_priv.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <numeric>
using namespace std;
//读入影像数据，输出影像长宽以及格式信息
GDALDataset* Read_image(GDALDataset* pDatasetRead, char* filename);
//CInt16生成强度图
vector<float> intensity(GDALDataset* pDatasetRead);
//转Db
vector<float> to_dB(vector<float> intensity, int width, int height);
//0-255可视化
vector<float> visible(vector<float> db, int width, int height);
//多视
vector<float> multi_view(vector<float> visible, int width, int height, int ground_look, int azimuth_look);

int main()
{
    GDALAllRegister(); //注册GDAL库中的所有驱动程序
    GDALDataset* pDatasetRead{};
    GDALDataset* pDatasetSave;
    GDALDriver* pDriver;
    //char filename[200] = "data/imagery_HH.tif";
    //char filename[200] = "data/IMAGE_HH_SRA_strip_007.cos";


    char filename[200] = "data/visible.tif";
    pDatasetRead = (GDALDataset*)GDALOpen(filename, GA_ReadOnly);
    GDALRasterBand* band = pDatasetRead->GetRasterBand(1);
    GDALDataType datatype = band->GetRasterDataType();
    // 获取波段的宽度和高度
    int xSize = band->GetXSize();
    int ySize = band->GetYSize();
    // 分配内存用于存储实部和虚部
    vector<float> qdArray(xSize * ySize);
    // 读取波段数据
    CPLErr eErr = band->RasterIO(GF_Read, 0, 0, xSize, ySize, qdArray.data(), xSize, ySize, datatype, 0, 0);


    //pDatasetRead = Read_image(pDatasetRead, filename);
    //GDALDataType datatype = pDatasetRead->GetRasterBand(1)->GetRasterDataType();
    //int width = pDatasetRead->GetRasterXSize();
    //int height = pDatasetRead->GetRasterYSize();
    //if (pDatasetRead == NULL) {
    //    return 1;
    //}
    ////根据影像存储格式进行强度图计算
    //vector<float> qdArray = intensity(pDatasetRead);
    //if (qdArray.size()==0) {
    //    return 1;
    //}
    ////转换成分贝
    //vector<float> dbArray = to_dB(qdArray, width, height);
    ////转换至0-255区间
    //vector<float> vArray = visible(dbArray, width, height);
    //多视
    //terrasar-x的参数
    float range_resolution = 2.27785611941735544;
    float azimuth_resolution = 3.29999995231628418;
    float angle = 31.1733936943751999;
    int ground_look = 4;
    int azimuth_look = 3;
    //radasat的参数
    //float range_resolution = 2.27785611941735544;
    //float azimuth_resolution = 3.29999995231628418;
    //float angle = 3.11733936943751999;
    cout << "距离向视数：" << ground_look << endl;
    cout << "方位向视数：" << azimuth_look << endl;
    vector<float> mvArray = multi_view(qdArray, xSize, ySize, ground_look, azimuth_look);
    //vector<float> mvArray = multi_view(vArray, width, height, ground_look, azimuth_look);

   
    //释放内存和关闭数据集
    GDALClose(pDatasetRead);

}


GDALDataset* Read_image(GDALDataset* pDatasetRead, char* filename) {
    pDatasetRead = (GDALDataset*)GDALOpen(filename, GA_ReadOnly);
    if (pDatasetRead == NULL) {
        printf("Failed to open file: %s\n", filename);
        return pDatasetRead;
    }
    int lWidth = pDatasetRead->GetRasterXSize();
    int lHeight = pDatasetRead->GetRasterYSize();
    int nBands = pDatasetRead->GetRasterCount();
    GDALDataType datatype = pDatasetRead->GetRasterBand(1)->GetRasterDataType();
    cout << "width: " << lWidth << endl;
    cout << "height: " << lHeight << endl;
    cout << "Bands: " << nBands << endl;
    cout << "DataType: " << GDALGetDataTypeName(datatype) << endl;
    return pDatasetRead;
}
vector<float> intensity(GDALDataset* pDatasetRead) {
    GDALRasterBand* poBand = pDatasetRead->GetRasterBand(1); // 获取第一个波段
    GDALDataType datatype = poBand->GetRasterDataType();
    vector<float> err;
    if (strcmp(GDALGetDataTypeName(datatype), "CInt16")==0) {
        // 获取第一个波段
        GDALRasterBand* band = pDatasetRead->GetRasterBand(1);
        if (!band)
        {
            cout << "无法获取CInt16波段" << endl;
            return err;
        }
        // 获取波段的宽度和高度
        int xSize = band->GetXSize();
        int ySize = band->GetYSize();
        // 分配内存用于存储实部和虚部
        vector<short> realArray(xSize * ySize);
        vector<short> imgArray(xSize * ySize);
        vector<int> tmpArray(xSize * ySize);
        vector<float> qdArray(xSize * ySize);
        // 读取波段数据
        CPLErr eErr = band->RasterIO(GF_Read, 0, 0, xSize, ySize, tmpArray.data(), xSize, ySize, datatype, 0, 0);
        // 分离实部和虚部
        for (size_t i = 0; i < tmpArray.size(); ++i)
        {
            int value = tmpArray[i];
            realArray[i] = (short)(value >> 16); // 实部
            imgArray[i] = (short)(value & 0xffff); // 虚部
            qdArray[i] = sqrt(realArray[i] * realArray[i] + imgArray[i] * imgArray[i]);
        }
        //存成tif影像
        //获取一个GTIFF格式的驱动程序，创建一个新的GTIFF格式的数据集
        GDALDriver* pDriver = GetGDALDriverManager()->GetDriverByName("GTIFF");
        char** papszOptions = pDriver->GetMetadata();
        GDALDataset* pDatasetSave = pDriver->Create("data/terrasar-x_intesnsity.tif", xSize, ySize, 1, GDT_Float32, papszOptions);
        if (pDatasetSave == NULL) {
            cout << "输出路径创建失败"<<endl;
            GDALClose(pDatasetRead);
            return err;
        }
        //将图像数据写入到新的数据集中
        if (pDatasetSave->RasterIO(GF_Write, 0, 0, xSize, ySize, qdArray.data(), xSize, ySize, GDT_Float32, 1, NULL, 0, 0, 0) != CE_None) {
            cout<<"tif写入失败"<<endl;
            GDALClose(pDatasetRead);
            GDALClose(pDatasetSave);
        }
        return qdArray;
    }
    else if (strcmp(GDALGetDataTypeName(datatype), "Int16") == 0) {
        //读取图像数据
        int lWidth, lHeight, nBands;
        lWidth = pDatasetRead->GetRasterXSize();
        lHeight = pDatasetRead->GetRasterYSize();
        nBands = pDatasetRead->GetRasterCount();
        //获取波段
        GDALRasterBand* poBand1 = pDatasetRead->GetRasterBand(1); // 获取第一个波段
        GDALRasterBand* poBand2 = pDatasetRead->GetRasterBand(2); // 获取第一个波段
        vector<short> realArray(lWidth * lHeight);
        vector<short> imgArray(lWidth * lHeight);
        vector<int> tmpArray(lWidth * lHeight);
        vector<float> qdArray(lWidth * lHeight);
        poBand1->RasterIO(GF_Read, 0, 0, lWidth, lHeight, realArray.data(), lWidth, lHeight, datatype, 0, 0);
        poBand2->RasterIO(GF_Read, 0, 0, lWidth, lHeight, imgArray.data(), lWidth, lHeight, datatype, 0, 0);
        //计算强度
        for (size_t i = 0; i < tmpArray.size(); ++i)
        {
            int value = tmpArray[i];
            qdArray[i] = sqrt(realArray[i] * realArray[i] + imgArray[i] * imgArray[i]);
            //qdArray[i] = 10 * log10(qdArray[i]);
        }
        //存成tif影像
        //获取一个GTIFF格式的驱动程序，创建一个新的GTIFF格式的数据集
        GDALDriver* pDriver = GetGDALDriverManager()->GetDriverByName("GTIFF");
        char** papszOptions = pDriver->GetMetadata();
        GDALDataset* pDatasetSave = pDriver->Create("data/radasat_intensity.tif", lWidth, lHeight, 1, GDT_Float32, papszOptions);
        if (pDatasetSave == NULL) {
            cout << "输出路径创建失败" << endl;
            GDALClose(pDatasetRead);
            return err;
        }
        //将图像数据写入到新的数据集中
        if (pDatasetSave->RasterIO(GF_Write, 0, 0, lWidth, lHeight, qdArray.data(), lWidth, lHeight, GDT_Float32, 1, NULL, 0, 0, 0) != CE_None) {
            cout << "tif写入失败" << endl;
            GDALClose(pDatasetRead);
            GDALClose(pDatasetSave);
        }
        return qdArray;
        
    }
    else {
        cout << "文件格式错误";
        return err;
    }
}
vector<float> to_dB(vector<float> intensity, int width, int height) {
    vector<float> dbArray(intensity.size());
    for (int i = 0; i < intensity.size(); i++) {
        dbArray[i] = 10 * log10(intensity[i]);
    }
    //存成tif影像
        //获取一个GTIFF格式的驱动程序，创建一个新的GTIFF格式的数据集
    GDALDriver* pDriver = GetGDALDriverManager()->GetDriverByName("GTIFF");
    char** papszOptions = pDriver->GetMetadata();
    GDALDataset* pDatasetSave = pDriver->Create("data/db.tif", width, height, 1, GDT_Float32, papszOptions);
    if (pDatasetSave == NULL) {
        cout << "dB输出路径创建失败" << endl;
        return dbArray;
    }
    //将图像数据写入到新的数据集中
    if (pDatasetSave->RasterIO(GF_Write, 0, 0, width, height, dbArray.data(), width, height, GDT_Float32, 1, NULL, 0, 0, 0) != CE_None) {
        cout << "dB tif写入失败" << endl;
        GDALClose(pDatasetSave);
    }
    return dbArray;
}
vector<float> visible(vector<float> db, int width, int height) {
    vector<float> vArray(db.size());
    // 找到最大值和最小值
    float maxVal = *max_element(db.begin(), db.end());
    for (int i = 0; i < vArray.size(); i++) {
        vArray[i] = (db[i] - 0) / (maxVal - 0) * 255;
    }
    //存成tif影像
        //获取一个GTIFF格式的驱动程序，创建一个新的GTIFF格式的数据集
    GDALDriver* pDriver = GetGDALDriverManager()->GetDriverByName("GTIFF");
    char** papszOptions = pDriver->GetMetadata();
    GDALDataset* pDatasetSave = pDriver->Create("data/visible.tif", width, height, 1, GDT_Float32, papszOptions);
    if (pDatasetSave == NULL) {
        cout << "v-输出路径创建失败" << endl;
        return vArray;
    }
    //将图像数据写入到新的数据集中
    if (pDatasetSave->RasterIO(GF_Write, 0, 0, width, height, vArray.data(), width, height, GDT_Float32, 1, NULL, 0, 0, 0) != CE_None) {
        cout << "v-tif写入失败" << endl;
    }
    return vArray;
}
vector<float> multi_view(vector<float> visible, int width, int height, int ground_look, int azimuth_look){
    vector<float> err(visible.size());
    //定义多视图大小
    int mv_width = width / ground_look;
    int mv_height = height / azimuth_look;
    vector<float> mvArray(mv_width * mv_height);
    vector<float> mvArray_w(mv_width * height);//距离向抽稀半成品

    //距离向抽稀
    for(int i =0;i<height;i++){
		for(int j =0;j<mv_width;j++){
            //求平均
            float sum = 0;
            for (int m = 0; m < ground_look; m++) {
                sum += visible[i * width + ground_look * j + m];
            }
            mvArray_w[i * mv_width + j] = sum / ground_look;
		}
	}
    //方位向抽稀
    for (int i = 0; i < mv_width; i++) {
        for (int j = 0; j < mv_height; j++) {
            float sum = 0;
            for (int m = 0; m < azimuth_look; m++) {
                sum += mvArray_w[(azimuth_look * j + m) * mv_width + i];
            }
            mvArray[i + j * mv_width] = sum / azimuth_look;
        }
    }
    //存成tif影像
	//获取一个GTIFF格式的驱动程序，创建一个新的GTIFF格式的数据集
    GDALDriver* pDriver = GetGDALDriverManager()->GetDriverByName("GTIFF");
    char** papszOptions = pDriver->GetMetadata();
    GDALDataset* pDatasetSave = pDriver->Create("data/multi.tif", mv_width, mv_height, 1, GDT_Float32, papszOptions);
    if (pDatasetSave == NULL) {
        cout << "multi-输出路径创建失败" << endl;
        return mvArray;
    }
    //将图像数据写入到新的数据集中
    if (pDatasetSave->RasterIO(GF_Write, 0, 0, mv_width, mv_height, mvArray.data(), mv_width, mv_height, GDT_Float32, 1, NULL, 0, 0, 0) != CE_None) {
        cout << "multi-tif写入失败" << endl;
    }
    return mvArray;
}



