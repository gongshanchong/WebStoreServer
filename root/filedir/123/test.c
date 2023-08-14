#include <stdio.h>
#include <stdlib.h>

//将两个数组归并
int * Merge(int a[], int m, int b[], int n){
    int *ab = (int *)malloc((m+n+1) * sizeof (int));
    int i = 0, j = 0, k = 0;
    while(i < m && j < n) {
        if (a[i] < b[j]) {
            ab[k++] = a[i++];
        }else{
            ab[k++] = b[j++];
        }
    }
    while(i < m && j == n){
        ab[k++] = a[i++];
    }
    while(j < n && i == m){
        ab[k++] = b[j++];
    }
    return ab;
}

//去掉相同的数
int deleteSame(int arr[], int n){
    int index = 0;
    for(int i = 1; i < n-index; ++i){
        if(arr[i-1] == arr[i]){
            int temp = arr[i];
            for(int j = i; j < n-1; ++j){
                arr[j] = arr[j+1];
            }
            arr[n-1-index] = temp;
            index++;
        }
    }
    return n-index;
}

int cmp(const void *a, const void *b){
    return *(int *)a - *(int *)b;
}

int main() {
    int arr[200][50];
    int colNum[200];
    int num;
    int row = 0, col = 0;

    // char test[300];
    // while(fgets(test, 300, stdin))
    // {
    //     printf("%s",test);
    // }
    while(scanf("%d", &num) != EOF){
        arr[row][col++] = num;
        char c = getchar();
//        换行对应的ASCII码值为10，当碰到换行符，二维数组也进行换行存储
        //printf("%d ", num);
        if(c == '\n'){
//        对每一行数据进行快排
            qsort(arr[row], col, sizeof(int), cmp);
            colNum[row] = col; //存储一行有多少个数
            row++;
            col = 0;  //另起一行从第一列开始存数
            //printf("\n");
        }
    }

    for(int i = 0; i < (row+1) / 2; ++i){
//        将2i和2i+1行进行归并
        int *mge = Merge(arr[2*i], colNum[2*i], arr[2*i+1], colNum[2*i+1]);
        int cnt = deleteSame(mge, colNum[2*i] + colNum[2*i + 1]);
        for(int j = 0; j < cnt; ++j){
            printf("%d", mge[j]);
            if(j!=cnt-1) printf(" ");
        }
        printf("\n");
    }
    return 0;

}
