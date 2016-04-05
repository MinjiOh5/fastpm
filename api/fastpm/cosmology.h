#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    double OmegaM;
    double OmegaLambda;
} Cosmology;

double 
GrowthFactor(double a, Cosmology c);

double 
GrowthFactor2(double a, Cosmology c);

double
GrowthFactor2v(double a, Cosmology c);

double DprimeQ(double a, double nGrowth, Cosmology c);
double HubbleEa(double a, Cosmology c);
double OmegaA(double a, Cosmology c);

#ifdef __cplusplus
}
#endif
