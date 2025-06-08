

// 由于影像数据过大，data文件夹中并没有原始影像数据，如需运行本代码需要将原始影像数据放入data文件夹中，由于滤波区域较小，保留在data文件夹下
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
//均值滤波
vector<unsigned char> meanFilter(GDALDataset* pDatasetRead, int kernalx, int kernaly);
//Lee滤波
vector<unsigned char> LeeFilter(GDALDataset* pDatasetRead, int kernalx, int kernaly);
//增强Lee滤波
vector<unsigned char> Ad_LeeFilter(GDALDataset* pDatasetRead, int kernalx, int kernaly);
//Kuan滤波
vector<unsigned char> KuanFilter(GDALDataset* pDatasetRead, int kernalx, int kernaly);
//增强Kuan滤波
vector<unsigned char> AdKuanFilter(GDALDataset* pDatasetRead, int kernalx, int kernaly);
//Frost滤波
vector<unsigned char> FrostFilter(GDALDataset* pDatasetRead, int kernalx, int kernaly);
//增强Frost滤波
vector<unsigned char> AdFrostFilter(GDALDataset* pDatasetRead, int kernalx, int kernaly);


int main()
{
    GDALAllRegister(); //注册GDAL库中的所有驱动程序
    GDALDataset* pDatasetRead{};
    GDALDataset* pDatasetSave;
    GDALDriver* pDriver;
    // 影像预处理
    // 由于影像数据过大，data文件夹中并没有原始影像数据，如需运行本代码需要将原始影像数据放入data文件夹中，由于滤波区域较小，保留在data文件夹下
    char filename[200] = "data/imagery_HH.tif";
    //char filename[200] = "data/IMAGE_HH_SRA_strip_007.cos";
    pDatasetRead = Read_image(pDatasetRead, filename); //读取COSAR影像
    if (pDatasetRead == NULL) {
        return 1;
    }
    GDALDataType datatype = pDatasetRead->GetRasterBand(1)->GetRasterDataType();
    int width = pDatasetRead->GetRasterXSize();
    int height = pDatasetRead->GetRasterYSize();
    vector<float> qdArray = intensity(pDatasetRead);// 计算强度图
    if (qdArray.size()==0) {
        return 1;
    }
    vector<float> dbArray = to_dB(qdArray, width, height);    // 计算分贝
    vector<float> vArray = visible(dbArray, width, height); // 线性变换至0-255区间
    int ground_look = 3; //距离向视数
    int azimuth_look = 3; //方位向视数
    vector<float> mvArray = multi_view(qdArray, width, height, ground_look, azimuth_look); //多视处理

    // 由于原始影像太大，使用ENVI软件裁剪多视结果，对裁剪后的影像进行滤波处理
    char filename0[200] = "data/r2.tif"; // 待处理影像
    //char filename0[200] = "data/zhibei.tif"; // 待处理影像
    pDatasetRead = Read_image(pDatasetRead, filename0); //读取影像
    vector<unsigned char> Filter_img_mean = meanFilter(pDatasetRead, 3, 3);//均值滤波
    vector<unsigned char> Filter_img_Lee = LeeFilter(pDatasetRead, 3, 3);//Lee滤波
    vector<unsigned char> Filter_img_AdLee = Ad_LeeFilter(pDatasetRead, 3, 3);//增强Lee滤波
    vector<unsigned char> Filter_img_Kuan = KuanFilter(pDatasetRead, 3, 3);//Kuan滤波
    vector<unsigned char> Filter_img_AdKuan = AdKuanFilter(pDatasetRead, 3, 3);//Kuan滤波
    vector<unsigned char> Filter_img_Frost = FrostFilter(pDatasetRead, 3, 3);//Frost滤波
    vector<unsigned char> Filter_img_AdFrost = AdFrostFilter(pDatasetRead, 3, 3);//Frost滤波


    
   
    //释放内存和关闭数据集
    //Filter_img.clear();
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
        GDALDataset* pDatasetSave = pDriver->Create("data/intensity.tif", xSize, ySize, 1, GDT_Float32, papszOptions);
        if (pDatasetSave == NULL) {
            cout << "输出路径创建失败"<<endl;
            GDALClose(pDatasetRead);
            return err;
        }
        // 将原影像的投影信息和地理变换参数复制到新影像
        pDatasetSave->SetProjection(pDatasetRead->GetProjectionRef());
        double adfGeoTransform[6];
        pDatasetRead->GetGeoTransform(adfGeoTransform);
        pDatasetSave->SetGeoTransform(adfGeoTransform);
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
        GDALRasterBand* poBand2 = pDatasetRead->GetRasterBand(2); // 获取第二个波段
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
    vector<float> mvArray_w(mv_width * height);

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
vector<unsigned char> meanFilter(GDALDataset* pDatasetRead, int kernalx, int kernaly) {
    GDALRasterBand* poBand = pDatasetRead->GetRasterBand(1); // 获取第一个波段
    int width = pDatasetRead->GetRasterXSize();
    int height = pDatasetRead->GetRasterYSize();
    GDALDataType datatype = pDatasetRead->GetRasterBand(1)->GetRasterDataType();
    vector<unsigned char> qdArray(width * height);
    vector<unsigned char> filterArray(width * height);
    // 读取波段数据
    CPLErr eErr = poBand->RasterIO(GF_Read, 0, 0, width, height, qdArray.data(), width, height, datatype, 0, 0);
    // 给影像最外层赋值
    // 影像边缘不做处理
    for (int i = 0; i < height; i++)
        for (int j = 0; j < width; j++)
            if ((i >= 0 && i <= kernaly) || (j >= 0 && j <= kernalx) || 
                (i <= height - 1 && i >= height - 1 - kernaly) || 
                (j <= width - 1 && j >= width - 1 - kernalx))
                filterArray[i * width + j] = qdArray[i * width + j];
    // 初始化临时数组
    int num = kernalx * kernaly; 
    int* temp = new int[num];
    // 对图像中间部分进行均值滤波处理
    for (int i = (kernaly - 1) / 2; i < height - (kernaly - 1) / 2; i++) {
        for (int j = (kernalx - 1) / 2; j < width - (kernalx - 1) / 2; j++) {
            int media_x = (kernalx + 1) / 2;
            int media_y = (kernaly + 1) / 2;
            // 将滤波窗口内的像素值存储到临时数组temp中
            for (int m = 1; m <= kernalx; m++) {
                for (int n = 1; n <= kernaly; n++) {
                    int index = (n - 1) * kernalx + m - 1;
                    temp[index] = (int)qdArray[(i - (media_y - n)) * width + j - (media_x - m)];
                }
            }
            // 计算滤波窗口内所有像素值的总和
            double sum = 0;
            for (int k = 0; k < num; k++) { sum = sum + temp[k]; }
            // 计算平均值，并赋值给NewBuf
            double ave = sum / num; int m = i * width + j;
            filterArray[m] = (unsigned char)ave;
        }
    }
    delete[] temp; // 释放临时数组内存
    //存成tif影像
   //获取一个GTIFF格式的驱动程序，创建一个新的GTIFF格式的数据集
    GDALDriver* pDriver = GetGDALDriverManager()->GetDriverByName("GTIFF");
    char** papszOptions = pDriver->GetMetadata();
    GDALDataset* pDatasetSave = pDriver->Create("data/Filter-mean.tif", width, height, 1, GDT_Byte, papszOptions);
    if (pDatasetSave == NULL) {
        cout << "Filtered-输出路径创建失败" << endl;
        return filterArray;
    }
    //将图像数据写入到新的数据集中
    if (pDatasetSave->RasterIO(GF_Write, 0, 0, width, height, filterArray.data(), width, height, GDT_Byte, 1, NULL, 0, 0, 0) != CE_None) {
        cout << "filter写入失败" << endl;
    }
    return filterArray;
}
vector<unsigned char> LeeFilter(GDALDataset* pDatasetRead, int kernalx, int kernaly)
{
    GDALRasterBand* poBand = pDatasetRead->GetRasterBand(1); // 获取第一个波段
    int width = pDatasetRead->GetRasterXSize();
    int height = pDatasetRead->GetRasterYSize();
    GDALDataType datatype = pDatasetRead->GetRasterBand(1)->GetRasterDataType();
    vector<unsigned char> imgArray(width * height);//输入影像
    vector<unsigned char> filterArray(width * height);//输出影像
    vector<unsigned char> window(kernalx * kernaly); //滤波窗口
    CPLErr eErr = poBand->RasterIO(GF_Read, 0, 0, width, height, imgArray.data(), width, height, datatype, 0, 0);//读入影像
    int kernalx0 = (kernalx - 1) / 2; //窗口大小
    int kernaly0 = (kernaly - 1)/2; 
    // 影像边缘做处理
    for (int i = 0; i < height; i++)
        for (int j = 0; j < width; j++)
            if ((i >= 0 && i <= kernaly0) || (j >= 0 && j <= kernalx0) ||
                (i <= height - 1 && i >= height - 1 - kernaly0) ||
                (j <= width - 1 && j >= width - 1 - kernalx0))
                filterArray[i * width + j] = imgArray[i * width + j];
    //四重循环，循环影像XY和窗口XY
    for (int i = kernaly0 + 1; i < height - 1 - kernaly0; i++)
    {
        for (int j = kernalx0 + 1; j < width - 1 - kernalx0; j++)
        {
            //循环窗口内每个像元
            int count = 0;
            for (int x = -kernalx0; x <= kernalx0; x++)
            {
                for (int y = -kernaly0; y <= kernaly0; y++)
                {
                    window[count] = ((int)imgArray[(i + y) * width + j + x]);//取出窗口内每个像元
                    count++;
                }
            }
            //计算有关系数
            double sum = 0; 
            for (int i = 0; i < count; i++) sum += window[i];
            double mean = sum / count;//计算均值
            double S = 0; 
            for (int i = 0; i < count; i++) S += (window[i] - mean) * (window[i] - mean);
            double SS = S / count;//计算方差
            double C_u = 0.5227;//计算C_u，经过多视处理后视数为1
            double C_I = sqrt(SS) / mean;//计算C_I
            double w = 1 - (C_u * C_u) / (C_I * C_I);//计算w
            //灰度赋值
            double g = imgArray[i * width + j] * w + mean * (1 - w);//计算灰度值
            if (g > 255) g = 255; if (g < 0) g = 0;//去除异常值
            filterArray[i * width + j] = (int)g;//赋值给窗口中心
        }
    }
    //存成tif影像
   //获取一个GTIFF格式的驱动程序，创建一个新的GTIFF格式的数据集
    GDALDriver* pDriver = GetGDALDriverManager()->GetDriverByName("GTIFF");
    char** papszOptions = pDriver->GetMetadata();
    GDALDataset* pDatasetSave = pDriver->Create("data/Filter-Lee.tif", width, height, 1, GDT_Byte, papszOptions);
    if (pDatasetSave == NULL) {
        cout << "Filtered-输出路径创建失败" << endl;
        return filterArray;
    }
    //将图像数据写入到新的数据集中
    if (pDatasetSave->RasterIO(GF_Write, 0, 0, width, height, filterArray.data(), width, height, GDT_Byte, 1, NULL, 0, 0, 0) != CE_None) {
        cout << "filter写入失败" << endl;
    }
    return filterArray;
}
vector<unsigned char> Ad_LeeFilter(GDALDataset* pDatasetRead, int kernalx, int kernaly)
{
    GDALRasterBand* poBand = pDatasetRead->GetRasterBand(1); // 获取第一个波段
    int width = pDatasetRead->GetRasterXSize();
    int height = pDatasetRead->GetRasterYSize();
    GDALDataType datatype = pDatasetRead->GetRasterBand(1)->GetRasterDataType();
    vector<unsigned char> imgArray(width * height);//输入影像
    vector<unsigned char> filterArray(width * height);//输出影像
    vector<unsigned char> window(kernalx * kernaly); //滤波窗口
    CPLErr eErr = poBand->RasterIO(GF_Read, 0, 0, width, height, imgArray.data(), width, height, datatype, 0, 0);//读入影像
    kernalx = (kernalx - 1) / 2; //窗口大小
    kernaly = (kernaly - 1) / 2; //窗口大小
    // 影像边缘做处理
    for (int i = 0; i < height; i++)
        for (int j = 0; j < width; j++)
            if ((i >= 0 && i <= kernaly) || (j >= 0 && j <= kernalx) || (i <= height - 1 && i >= height - 1 - kernaly) || (j <= width - 1 && j >= width - 1 - kernalx))
                filterArray[i * width + j] = imgArray[i * width + j];
    //四重循环，循环影像XY和窗口XY
    for (int i = kernaly + 1; i < height - 1 - kernaly; i++)
    {
        for (int j = kernalx + 1; j < width - 1 - kernalx; j++)
        {
            //循环窗口内每个像元
            int count = 0;
            for (int x = -kernalx; x <= kernalx; x++)
            {
                for (int y = -kernaly; y <= kernaly; y++)
                {
                    window[count] = ((int)imgArray[(i + y) * width + j + x]);//取出窗口内每个像元
                    count++;
                }
            }
            //计算有关系数
            double sum = 0; for (int i = 0; i < count; i++) sum += window[i];
            double mean = sum / count;//计算均值
            double sum2 = 0; for (int i = 0; i < count; i++) sum2 += (window[i] - mean) * (window[i] - mean);
            double variance = sum2 / count;//计算方差
            double C_u = 0.5227;//计算C_u，经过多视处理后视数为1
            double C_max = sqrt(3) * C_u;//计算C_max
            double C_I = sqrt(variance) / mean;//计算C_I
            double w = 1 - (C_u * C_u) / (C_I * C_I);//计算w
            //阈值判断
            double g = 0;
            if (C_I < C_u) g = mean;
            else if (C_I >= C_u && C_I <= C_max) g = window[(count + 1) / 2 - 1] * w + mean * (1 - w);
            else g = window[(count + 1) / 2 - 1];
            //灰度赋值
            if (g > 255) g = 255; if (g < 0) g = 0;//去除异常值
            double a = imgArray[i * width + j];
            filterArray[i * width + j] = g;//赋值给窗口中心
        }
    }
    //存成tif影像
   //获取一个GTIFF格式的驱动程序，创建一个新的GTIFF格式的数据集
    GDALDriver* pDriver = GetGDALDriverManager()->GetDriverByName("GTIFF");
    char** papszOptions = pDriver->GetMetadata();
    GDALDataset* pDatasetSave = pDriver->Create("data/Filter-AdLee.tif", width, height, 1, GDT_Byte, papszOptions);
    if (pDatasetSave == NULL) {
        cout << "Filtered-输出路径创建失败" << endl;
        return filterArray;
    }
    //将图像数据写入到新的数据集中
    if (pDatasetSave->RasterIO(GF_Write, 0, 0, width, height, filterArray.data(), width, height, GDT_Byte, 1, NULL, 0, 0, 0) != CE_None) {
        cout << "filter写入失败" << endl;
    }
    return filterArray;
    
}
vector<unsigned char> KuanFilter(GDALDataset* pDatasetRead, int kernalx, int kernaly)
{
    GDALRasterBand* poBand = pDatasetRead->GetRasterBand(1); // 获取第一个波段
    int width = pDatasetRead->GetRasterXSize();
    int height = pDatasetRead->GetRasterYSize();
    GDALDataType datatype = pDatasetRead->GetRasterBand(1)->GetRasterDataType();
    vector<unsigned char> imgArray(width * height);//输入影像
    vector<unsigned char> filterArray(width * height);//输出影像
    vector<unsigned char> window(kernalx * kernaly); //滤波窗口
    CPLErr eErr = poBand->RasterIO(GF_Read, 0, 0, width, height, imgArray.data(), width, height, datatype, 0, 0);//读入影像
    kernalx = (kernalx - 1) / 2; //窗口大小
    kernaly = (kernaly - 1) / 2; //窗口大小
    // 影像边缘做处理
    for (int i = 0; i < height; i++)
        for (int j = 0; j < width; j++)
            if ((i >= 0 && i <= kernaly) || (j >= 0 && j <= kernalx) || (i <= height - 1 && i >= height - 1 - kernaly) || (j <= width - 1 && j >= width - 1 - kernalx))
                filterArray[i * width + j] = imgArray[i * width + j];
    //四重循环，循环影像XY和窗口XY
    for (int i = kernaly + 1; i < height - 1 - kernaly; i++)
    {
        for (int j = kernalx + 1; j < width - 1 - kernalx; j++)
        {
            //循环窗口内每个像元
            int count = 0;
            for (int x = -kernalx; x <= kernalx; x++)
            {
                for (int y = -kernaly; y <= kernaly; y++)
                {
                    window[count] = ((int)imgArray[(i + y) * width + j + x]);//取出窗口内每个像元
                    count++;
                }
            }
            //计算有关系数
            double sum = 0; for (int i = 0; i < count; i++) sum += window[i];
            double mean = sum / count;//计算均值
            double sum2 = 0; for (int i = 0; i < count; i++) sum2 += (window[i] - mean) * (window[i] - mean);
            double variance = sum2 / count;//计算方差
            double C_u = 1 /sqrt(3);//计算C_u
            double C_I = sqrt(variance) / mean;//计算C_I
            double w = (1 - (C_u * C_u) / (C_I * C_I)) / (1 + C_u * C_u);//计算w
            //灰度赋值
            double g = window[(count + 1) / 2 - 1] * w + mean * (1 - w);//计算灰度值
            if (g > 255) g = 255; if (g < 0) g = 0;//去除异常值
            filterArray[i * width + j] = (int)g;//赋值给窗口中心
            //int m = i * width + j;
            //double no = (double)filterArray[m] - (double)imgArray[m];
            //derta = derta + no * no;
        }

    }
    //存成tif影像
   //获取一个GTIFF格式的驱动程序，创建一个新的GTIFF格式的数据集
    GDALDriver* pDriver = GetGDALDriverManager()->GetDriverByName("GTIFF");
    char** papszOptions = pDriver->GetMetadata();
    GDALDataset* pDatasetSave = pDriver->Create("data/Filter-Kuan.tif", width, height, 1, GDT_Byte, papszOptions);
    if (pDatasetSave == NULL) {
        cout << "Filtered-输出路径创建失败" << endl;
        return filterArray;
    }
    //将图像数据写入到新的数据集中
    if (pDatasetSave->RasterIO(GF_Write, 0, 0, width, height, filterArray.data(), width, height, GDT_Byte, 1, NULL, 0, 0, 0) != CE_None) {
        cout << "filter写入失败" << endl;
    }
    return filterArray;
}
vector<unsigned char> AdKuanFilter(GDALDataset* pDatasetRead, int kernalx, int kernaly)
{
    int look = 3; //视数
    GDALRasterBand* poBand = pDatasetRead->GetRasterBand(1); // 获取第一个波段
    int width = pDatasetRead->GetRasterXSize();
    int height = pDatasetRead->GetRasterYSize();
    GDALDataType datatype = pDatasetRead->GetRasterBand(1)->GetRasterDataType();
    vector<unsigned char> imgArray(width * height);//输入影像
    vector<unsigned char> filterArray(width * height);//输出影像
    vector<unsigned char> window(kernalx * kernaly); //滤波窗口
    CPLErr eErr = poBand->RasterIO(GF_Read, 0, 0, width, height, imgArray.data(), width, height, datatype, 0, 0);//读入影像
    kernalx = (kernalx - 1) / 2; //窗口大小
    kernaly = (kernaly - 1) / 2; //窗口大小
    // 影像边缘做处理
    for (int i = 0; i < height; i++)
        for (int j = 0; j < width; j++)
            if ((i >= 0 && i <= kernaly) || (j >= 0 && j <= kernalx) || (i <= height - 1 && i >= height - 1 - kernaly) || (j <= width - 1 && j >= width - 1 - kernalx))
                filterArray[i * width + j] = imgArray[i * width + j];
    //四重循环，循环影像XY和窗口XY
    for (int i = kernaly + 1; i < height - 1 - kernaly; i++)
    {
        for (int j = kernalx + 1; j < width - 1 - kernalx; j++)
        {
            //循环窗口内每个像元
            int count = 0;
            for (int x = -kernalx; x <= kernalx; x++)
            {
                for (int y = -kernaly; y <= kernaly; y++)
                {
                    window[count] = ((int)imgArray[(i + y) * width + j + x]);
                    count++;
                }
            }
            //计算有关系数
            double sum = 0; for (int i = 0; i < count; i++) sum += window[i];
            double mean = sum / count;//计算均值
            double sum2 = 0; for (int i = 0; i < count; i++) sum2 += (window[i] - mean) * (window[i] - mean);
            double variance = sum2 / count;//计算方差
            double C_u = 1 / sqrt(look);//计算C_u
            double C_I = sqrt(variance) / mean;//计算C_I
            double w = (1 - (C_u * C_u) / (C_I * C_I)) / (1 + C_u * C_u);//计算w
            double C_max = sqrt(1 + 2 / look);//计算C_max
            //阈值判断
            double g = 0;
            if (C_I < C_u) 
                g = mean;
            else if (C_I >= C_u && C_I <= C_max) 
                g = window[(count + 1) / 2 - 1] * w + mean * (1 - w);
            else 
                g = window[(count + 1) / 2 - 1];
            //灰度赋值
            if (g > 255) g = 255; if (g < 0) g = 0;//去除异常值
            filterArray[i * width + j] = (int)g;//赋值给窗口中心
        }

    }
    //存成tif影像
   //获取一个GTIFF格式的驱动程序，创建一个新的GTIFF格式的数据集
    GDALDriver* pDriver = GetGDALDriverManager()->GetDriverByName("GTIFF");
    char** papszOptions = pDriver->GetMetadata();
    GDALDataset* pDatasetSave = pDriver->Create("data/Filter-AdKuan.tif", width, height, 1, GDT_Byte, papszOptions);
    if (pDatasetSave == NULL) {
        cout << "Filtered-输出路径创建失败" << endl;
        return filterArray;
    }
    //将图像数据写入到新的数据集中
    if (pDatasetSave->RasterIO(GF_Write, 0, 0, width, height, filterArray.data(), width, height, GDT_Byte, 1, NULL, 0, 0, 0) != CE_None) {
        cout << "filter写入失败" << endl;
    }
    return filterArray;
}
vector<unsigned char> FrostFilter(GDALDataset* pDatasetRead, int kernalx, int kernaly)
{
    int look = 3; //视数
    GDALRasterBand* poBand = pDatasetRead->GetRasterBand(1); // 获取第一个波段
    int width = pDatasetRead->GetRasterXSize();
    int height = pDatasetRead->GetRasterYSize();
    GDALDataType datatype = pDatasetRead->GetRasterBand(1)->GetRasterDataType();
    vector<unsigned char> imgArray(width * height);//输入影像
    vector<unsigned char> filterArray(width * height);//输出影像
    vector<unsigned char> window(kernalx * kernaly); //滤波窗口
    CPLErr eErr = poBand->RasterIO(GF_Read, 0, 0, width, height, imgArray.data(), width, height, datatype, 0, 0);//读入影像
    kernalx = (kernalx - 1) / 2; //窗口大小
    kernaly = (kernaly - 1) / 2; //窗口大小
    // 影像边缘做处理
    for (int i = 0; i < height; i++)
        for (int j = 0; j < width; j++)
            if ((i >= 0 && i <= kernaly) || (j >= 0 && j <= kernalx) || (i <= height - 1 && i >= height - 1 - kernaly) || (j <= width - 1 && j >= width - 1 - kernalx))
                filterArray[i * width + j] = imgArray[i * width + j];
    //四重循环，循环影像XY和窗口XY
    for (int i = kernaly + 1; i < height - 1 - kernaly; i++)
    {
        for (int j = kernalx + 1; j < width - 1 - kernalx; j++)
        {
            //循环窗口内每个像元
            int count = 0;
            for (int x = -kernalx; x <= kernalx; x++)
            {
                for (int y = -kernaly; y <= kernaly; y++)
                {
                    window[count] = ((int)imgArray[(i + y) * width + j + x]);//取出窗口内每个像元
                    count++;
                }
            }
            //计算有关系数
            double sum = 0; for (int i = 0; i < count; i++) sum += window[i];
            double mean = sum / count;//计算均值
            double sum2 = 0; for (int i = 0; i < count; i++) sum2 += (window[i] - mean) * (window[i] - mean);
            double variance = sum2 / count;//计算方差
            //再次循环窗口内每个像元
            double A = 0, T = 0, M = 0, sum3 = 0, sum4 = 0;
            for (int x = -kernalx; x <= kernalx; x++)
            {
                for (int y = -kernaly; y <= kernaly; y++)
                {
                    A = variance / (mean * mean);//计算A
                    T = sqrt(x * x + y * y);//计算T
                    M = exp(-A * T);//计算M
                    sum3 += (int)imgArray[(i + y) * width + j + x] * M;
                    sum4 += M;
                }
            }
            //灰度赋值
            double g = sum3 / sum4;//计算灰度值
            if (g > 255) g = 255; if (g < 0) g = 0;//去除异常值
            filterArray[i * width + j] = (int)g;//赋值给窗口中心
        }
    }
    //存成tif影像
    //获取一个GTIFF格式的驱动程序，创建一个新的GTIFF格式的数据集
    GDALDriver* pDriver = GetGDALDriverManager()->GetDriverByName("GTIFF");
    char** papszOptions = pDriver->GetMetadata();
    GDALDataset* pDatasetSave = pDriver->Create("data/Filter-Frost.tif", width, height, 1, GDT_Byte, papszOptions);
    if (pDatasetSave == NULL) {
        cout << "Filtered-输出路径创建失败" << endl;
        return filterArray;
    }
    //将图像数据写入到新的数据集中
    if (pDatasetSave->RasterIO(GF_Write, 0, 0, width, height, filterArray.data(), width, height, GDT_Byte, 1, NULL, 0, 0, 0) != CE_None) {
        cout << "filter写入失败" << endl;
    }
}
vector<unsigned char> AdFrostFilter(GDALDataset* pDatasetRead, int kernalx, int kernaly)
{
    int look = 3; //视数
    GDALRasterBand* poBand = pDatasetRead->GetRasterBand(1); // 获取第一个波段
    int width = pDatasetRead->GetRasterXSize();
    int height = pDatasetRead->GetRasterYSize();
    GDALDataType datatype = pDatasetRead->GetRasterBand(1)->GetRasterDataType();
    vector<unsigned char> imgArray(width * height);//输入影像
    vector<unsigned char> filterArray(width * height);//输出影像
    vector<unsigned char> window(kernalx * kernaly); //滤波窗口
    CPLErr eErr = poBand->RasterIO(GF_Read, 0, 0, width, height, imgArray.data(), width, height, datatype, 0, 0);//读入影像
    kernalx = (kernalx - 1) / 2; //窗口大小
    kernaly = (kernaly - 1) / 2; //窗口大小
    // 影像边缘做处理
    for (int i = 0; i < height; i++)
        for (int j = 0; j < width; j++)
            if ((i >= 0 && i <= kernaly) || (j >= 0 && j <= kernalx) || (i <= height - 1 && i >= height - 1 - kernaly) || (j <= width - 1 && j >= width - 1 - kernalx))
                filterArray[i * width + j] = imgArray[i * width + j];
    //四重循环，循环影像XY和窗口XY
    for (int i = kernaly + 1; i < height - 1 - kernaly; i++)
    {
        for (int j = kernalx + 1; j < width - 1 - kernalx; j++)
        {
            //循环窗口内每个像元
            int count = 0;
            for (int x = -kernalx; x <= kernalx; x++)
            {
                for (int y = -kernaly; y <= kernaly; y++)
                {
                    window[count] = ((int)imgArray[(i + y) * width + j + x]);//取出窗口内每个像元
                    count++;
                }
            }
            //计算有关系数
            double sum = 0; 
            for (int i = 0; i < count; i++) 
                sum += window[i];
            double mean = sum / count;//计算均值
            double sum2 = 0; 
            for (int i = 0; i < count; i++) 
                sum2 += (window[i] - mean) * (window[i] - mean);
            double variance = sum2 / count;//计算方差
            double C_u = 1 / sqrt(look);//计算C_u
            double C_I = sqrt(variance) / mean;//计算C_I
            double C_max = sqrt(1 + 2 / look);//计算C_max
            //阈值判断
            double g = 0;
            if (C_I < C_u) g = mean;
            else if (C_I >= C_u && C_I <= C_max)
            {
                //再次循环窗口内每个像元
                double A = 0, T = 0, M = 0, sum3 = 0, sum4 = 0;
                for (int x = -kernalx; x <= kernalx; x++)
                {
                    for (int y = -kernaly; y <= kernaly; y++)
                    {
                        A = variance / (mean * mean);//计算A
                        T = sqrt(x * x + y * y);//计算T
                        M = exp(-A * T);//计算M
                        sum3 += (int)imgArray[(i + y) * width + j + x] * M;
                        sum4 += M;
                    }
                }
                g = sum3 / sum4;
            }
            else g = window[(count + 1) / 2 - 1];
            //灰度赋值
            if (g > 255) 
                g = 255; 
            if (g < 0) 
                g = 0;//去除异常值
            filterArray[i * width + j] = (int)g;//赋值给窗口中心
        }
    }
    //存成tif影像
    //获取一个GTIFF格式的驱动程序，创建一个新的GTIFF格式的数据集
    GDALDriver* pDriver = GetGDALDriverManager()->GetDriverByName("GTIFF");
    char** papszOptions = pDriver->GetMetadata();
    GDALDataset* pDatasetSave = pDriver->Create("data/Filter-AdFrost.tif", width, height, 1, GDT_Byte, papszOptions);
    if (pDatasetSave == NULL) {
        cout << "Filtered-输出路径创建失败" << endl;
        return filterArray;
    }
    //将图像数据写入到新的数据集中
    if (pDatasetSave->RasterIO(GF_Write, 0, 0, width, height, filterArray.data(), width, height, GDT_Byte, 1, NULL, 0, 0, 0) != CE_None) {
        cout << "filter写入失败" << endl;
    }
}

