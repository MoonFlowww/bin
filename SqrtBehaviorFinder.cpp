#include <iostream>
#include <cstdlib>
#include <ctime>
#include <sstream>
#include <cmath>
#include <vector>

std::stringstream oss;
double train[][2] = {
    {42, 6.4807},
    {81, 9.25},
    {36, 6.1},
    {100, 10.5},
    {4, 2.0},
    {144, 12.8},
    {49, 7.3},
    {64, 8},
    {121, 11.15},
    {25, 5.75},
    {9, 3.0},
    {16, 4.2},
    {169, 13.6},
    {196, 14},
    {225, 15.35},
    {289, 17.05},
    {324, 18.4},
    {400, 20.8},
    {441, 21.55}
};

#define train_count (sizeof(train) / sizeof(train[0]))

// Prédiction en utilisant deux neurones
double predict(double x, double w1, double b1, double c, double d, double w2, double b2) {
    // Premier neurone : y1 = (log2(x + d))^c * w1 + b1
    double y1 = pow(log2(x + d), c) * w1 + b1;
    // Second neurone : y2 = y1 + x * w2 + b2
    double y2 = y1 + x * w2 + b2;
    return y2;
}

// Fonction de coût avec régularisation L2
double cost(double w1, double b1, double c, double d, double w2, double b2, double lambda) {
    double MSE = 0.0;

    for (size_t i = 0; i < train_count; ++i) {
        double x = train[i][0];
        double y = predict(x, w1, b1, c, d, w2, b2);
        double r = y - train[i][1];
        MSE += r * r;
    }
    MSE /= train_count;

    // Ajouter le terme de régularisation L2
    double L2_reg = lambda * (w1 * w1 + w2 * w2);
    return MSE + L2_reg;
}

void test(double w1, double b1, double c, double d, double w2, double b2) {
    std::vector<std::pair<double, double>> v = { {50.0, 7.0711}, {200.0, 14.1421}, {500.0, 22.3607} };
    for (auto& x : v) {
        double input = x.first;
        double target_bonus = x.second;
        double y_pred_bonus = predict(input, w1, b1, c, d, w2, b2);

        std::cout << "Input: " << input << std::endl;
        std::cout << "Prediction: " << y_pred_bonus << std::endl;
        std::cout << "Should be around: " << target_bonus << std::endl;
        std::cout << "Relative Error: " << (target_bonus - y_pred_bonus) / y_pred_bonus << std::endl;
        std::cout << "\n";
    }
}

// Nouvelle fonction pour analyser la contribution de chaque neurone
void analyze_neuron_contribution(double w1, double b1, double c, double d, double w2, double b2) {
    double total_output_y1 = 0.0;
    double total_output_y2 = 0.0;

    for (size_t i = 0; i < train_count; ++i) {
        double x = train[i][0];
        double y_true = train[i][1];

        // Calcul des sorties de chaque neurone
        double L = log2(x + d);
        double L_c = pow(L, c);
        double y1 = L_c * w1 + b1;          // Sortie du premier neurone
        double y2 = x * w2 + b2;            // Contribution du second neurone
        double y_pred = y1 + y2;            // Prédiction finale

        // Accumuler les valeurs absolues des sorties
        total_output_y1 += fabs(y1);
        total_output_y2 += fabs(y2);
    }

    double total_output = total_output_y1 + total_output_y2;
    double percent_n1 = (total_output_y1 / total_output) * 100.0;
    double percent_n2 = (total_output_y2 / total_output) * 100.0;

    std::cout << "\nAnalyse de la contribution des neurones :" << std::endl;
    std::cout << "Contribution moyenne du Neurone 1 : " << percent_n1 << "%" << std::endl;
    std::cout << "Contribution moyenne du Neurone 2 : " << percent_n2 << "%" << std::endl;
}

void update(int epoch) {
    srand(time(0));
    double w1 = (rand() / (double)RAND_MAX) * 10.0;
    double b1 = (rand() / (double)RAND_MAX) * 5.0;
    double c = (rand() / (double)RAND_MAX) * 5.0;
    double d = (rand() / (double)RAND_MAX) * 5.0;

    double w2 = (rand() / (double)RAND_MAX) * 0.1; // Initialiser w2 petit
    double b2 = (rand() / (double)RAND_MAX) * 0.1; // Initialiser b2 petit

    double rate = 1e-4; // Taux d'apprentissage ajusté

    double lambda = 0.01; // Force de régularisation

    // Paramètres de l'optimiseur Adam
    double beta1 = 0.9;
    double beta2 = 0.999;
    double eps = 1e-8;

    // Estimations des moments pour tous les paramètres
    double m_w1 = 0.0, v_w1 = 0.0;
    double m_b1 = 0.0, v_b1 = 0.0;
    double m_c = 0.0, v_c = 0.0;
    double m_d = 0.0, v_d = 0.0;

    double m_w2 = 0.0, v_w2 = 0.0;
    double m_b2 = 0.0, v_b2 = 0.0;

    double init_cost = cost(w1, b1, c, d, w2, b2, lambda);
    double prev_c = init_cost;

    // Sauvegarde des meilleurs paramètres
    double backup_w1 = w1, backup_b1 = b1, backup_c = c, backup_d = d;
    double backup_w2 = w2, backup_b2 = b2;

    for (size_t e = 1; e <= epoch; ++e) {
        // Calcul des gradients analytiques
        double grad_w1 = 0.0, grad_b1 = 0.0, grad_c = 0.0, grad_d = 0.0;
        double grad_w2 = 0.0, grad_b2 = 0.0;
        size_t n = train_count;

        for (size_t i = 0; i < n; ++i) {
            double x = train[i][0];
            double y_true = train[i][1];

            // Premier neurone
            double L = log2(x + d);
            double L_c = pow(L, c);
            double y1 = L_c * w1 + b1;

            // Second neurone
            double y_pred = y1 + x * w2 + b2;

            double error = y_pred - y_true;

            // Gradients pour le second neurone
            grad_w2 += (2.0 / n) * error * x;
            grad_b2 += (2.0 / n) * error;

            // Gradients pour le premier neurone
            double dy_pred_dy1 = 1.0;
            double dy1_dw1 = L_c;
            double dy1_db1 = 1.0;
            double dy1_dc = w1 * L_c * log(L);
            double dy1_dd = w1 * c * pow(L, c - 1) * (1 / ((x + d) * log(2)));

            grad_w1 += (2.0 / n) * error * dy_pred_dy1 * dy1_dw1;
            grad_b1 += (2.0 / n) * error * dy_pred_dy1 * dy1_db1;
            grad_c += (2.0 / n) * error * dy_pred_dy1 * dy1_dc;
            grad_d += (2.0 / n) * error * dy_pred_dy1 * dy1_dd;
        }

        // Ajouter la régularisation aux gradients
        grad_w1 += 2 * lambda * w1;
        grad_w2 += 2 * lambda * w2;

        // Mise à jour des estimations des moments
        m_w1 = beta1 * m_w1 + (1 - beta1) * grad_w1;
        m_b1 = beta1 * m_b1 + (1 - beta1) * grad_b1;
        m_c = beta1 * m_c + (1 - beta1) * grad_c;
        m_d = beta1 * m_d + (1 - beta1) * grad_d;

        m_w2 = beta1 * m_w2 + (1 - beta1) * grad_w2;
        m_b2 = beta1 * m_b2 + (1 - beta1) * grad_b2;

        v_w1 = beta2 * v_w1 + (1 - beta2) * grad_w1 * grad_w1;
        v_b1 = beta2 * v_b1 + (1 - beta2) * grad_b1 * grad_b1;
        v_c = beta2 * v_c + (1 - beta2) * grad_c * grad_c;
        v_d = beta2 * v_d + (1 - beta2) * grad_d * grad_d;

        v_w2 = beta2 * v_w2 + (1 - beta2) * grad_w2 * grad_w2;
        v_b2 = beta2 * v_b2 + (1 - beta2) * grad_b2 * grad_b2;

        // Calcul des estimations corrigées du biais
        double m_hat_w1 = m_w1 / (1 - pow(beta1, e));
        double m_hat_b1 = m_b1 / (1 - pow(beta1, e));
        double m_hat_c = m_c / (1 - pow(beta1, e));
        double m_hat_d = m_d / (1 - pow(beta1, e));

        double m_hat_w2 = m_w2 / (1 - pow(beta1, e));
        double m_hat_b2 = m_b2 / (1 - pow(beta1, e));

        double v_hat_w1 = v_w1 / (1 - pow(beta2, e));
        double v_hat_b1 = v_b1 / (1 - pow(beta2, e));
        double v_hat_c = v_c / (1 - pow(beta2, e));
        double v_hat_d = v_d / (1 - pow(beta2, e));

        double v_hat_w2 = v_w2 / (1 - pow(beta2, e));
        double v_hat_b2 = v_b2 / (1 - pow(beta2, e));

        // Mise à jour des paramètres
        w1 -= rate * m_hat_w1 / (sqrt(v_hat_w1) + eps);
        b1 -= rate * m_hat_b1 / (sqrt(v_hat_b1) + eps);
        c -= rate * m_hat_c / (sqrt(v_hat_c) + eps);
        d -= rate * m_hat_d / (sqrt(v_hat_d) + eps);

        w2 -= rate * m_hat_w2 / (sqrt(v_hat_w2) + eps);
        b2 -= rate * m_hat_b2 / (sqrt(v_hat_b2) + eps);

        double current_cost = cost(w1, b1, c, d, w2, b2, lambda);

        if (current_cost < prev_c) {
            backup_w1 = w1;
            backup_b1 = b1;
            backup_c = c;
            backup_d = d;
            backup_w2 = w2;
            backup_b2 = b2;
            prev_c = current_cost;
        }
    }

    double impr = ((init_cost - prev_c) / prev_c);
    std::cout << "\nAmélioration totale : " << impr * 100 << "%   ||   MPE : " << (impr / epoch) * 100 << "%\n";

    // Restaurer les meilleurs paramètres si nécessaire
    if (cost(backup_w1, backup_b1, backup_c, backup_d, backup_w2, backup_b2, lambda) < cost(w1, b1, c, d, w2, b2, lambda)) {
        std::cout << "-> Le modèle précédent est meilleur que la dernière version ! Restauration...\n";
        w1 = backup_w1;
        b1 = backup_b1;
        c = backup_c;
        d = backup_d;
        w2 = backup_w2;
        b2 = backup_b2;
    }

    std::cout << "Paramètres finaux:\n";
    std::cout << "Premier Neurone (Couche 1):\n";
    std::cout << "w1 -> " << w1 << ", b1 -> " << b1 << ", c -> " << c << ", d -> " << d << "\n";
    std::cout << "Second Neurone (Couche 2):\n";
    std::cout << "w2 -> " << w2 << ", b2 -> " << b2 << "\n";

    test(w1, b1, c, d, w2, b2);
    analyze_neuron_contribution(w1, b1, c, d, w2, b2); // Appel à la fonction d'analyse
}

int main() {
    int epoch = 400000;
    std::cout << "L'entraînement a commencé...\n";
    update(epoch);

    return 0;
}
