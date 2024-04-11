* step0: 二值化
* step1: 连通块染色 < bfs
* step2: 求黑色区域出发到每个有色像素点的距离 < bfs（其实只要求边缘）这一步有点多余。黑色区域距离为 $0$
* step3: 以有色质心作为圆心
* step4: 外接半径是同色距离为 1 的像素点到圆心的平均距离
* step5: 对每个圆，求其和所属染色块的 iou，排除相对于 $\mathop{\text{最}}\limits_{\text{所有圆}} \text{佳} \text{iou}$ 太小的圆，再取面积最大的。

## Run test cmd line

```sh
gcc light_recognition.c -o 1 && ./1 light2.ppm > 1.ppm && open 1.ppm
```
